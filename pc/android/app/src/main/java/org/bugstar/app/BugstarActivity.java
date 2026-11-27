package org.bugstar.app;

// patch 34: SDL's activity plus a crash report you can read without adb or the data folder. (patch 35: the report is written on a background thread.)
// On every start it saves the app's own recent log (the previous run's too) and any crash notes to Downloads/bugstar_log.txt,
// and it tells you on screen when the last run crashed. (An app can always read its own logcat; nothing else is needed.)

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.app.Activity;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.View;
import android.util.Log;
import android.provider.MediaStore;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.zip.CRC32;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;

import org.libsdl.app.SDLActivity;

public class BugstarActivity extends SDLActivity {
    private boolean crashedLastTime = false;
    private static final int SAVE_EXPORT = 4012, SAVE_IMPORT = 4013, SRAM_SIZE = 32768;
    private static final byte[] SAVE_MAGIC = {'B','G','S','T','A','R','1','\n'};
    private boolean savePickerBusy;
    private String transferPath;
    private byte[] exportData;

    // Called by the native game on its own thread. The byte array is an
    // independent snapshot of cartridge SRAM, not a live disk file.
    public void beginSaveTransfer(final boolean importing, final String path, final byte[] data) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                if (savePickerBusy || isFinishing()) return;
                if (path == null || (!importing && (data == null || data.length != SRAM_SIZE))) return;
                savePickerBusy = true; transferPath = path; exportData = data;
                try {
                    Intent intent = new Intent(importing ? Intent.ACTION_OPEN_DOCUMENT : Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType(importing ? "*/*" : "application/octet-stream");
                    if (!importing) intent.putExtra(Intent.EXTRA_TITLE, "BUGSTAR-cartridge.bgsave");
                    startActivityForResult(intent, importing ? SAVE_IMPORT : SAVE_EXPORT);
                    Log.i("BUGSTAR_SAVE", importing ? "picker launched import" : "picker launched export");
                } catch (Exception e) {
                    endTransfer("No compatible file picker");
                }
            }
        });
    }

    private void endTransfer(final String message) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                savePickerBusy = false; exportData = null; transferPath = null;
                if (message != null) Toast.makeText(BugstarActivity.this, message, Toast.LENGTH_LONG).show();
            }
        });
    }

    private static void writeSave(DataOutputStream out, byte[] raw) throws IOException {
        CRC32 crc = new CRC32(); crc.update(raw);
        out.write(SAVE_MAGIC); out.writeInt((int)crc.getValue()); out.write(raw); out.flush();
    }

    private static byte[] readSave(InputStream stream) throws IOException {
        if (stream == null) throw new IOException("No file");
        try (DataInputStream in = new DataInputStream(stream)) {
            byte[] magic = new byte[8]; in.readFully(magic);
            if (!Arrays.equals(magic, SAVE_MAGIC)) throw new IOException("Not a BUGSTAR cartridge backup");
            int checksum = in.readInt();
            byte[] raw = new byte[SRAM_SIZE]; in.readFully(raw);
            if (in.read() != -1) throw new IOException("Wrong backup length");
            CRC32 crc = new CRC32(); crc.update(raw);
            if ((int)crc.getValue() != checksum) throw new IOException("Backup checksum mismatch");
            return raw;
        }
    }

    private void stageRestore(final String path, final byte[] raw) {
        new Thread(new Runnable() {
            @Override public void run() {
                File temp = new File(path + ".restore.write"), dest = new File(path + ".restore");
                try {
                    try (FileOutputStream file = new FileOutputStream(temp);
                         DataOutputStream out = new DataOutputStream(file)) {
                        writeSave(out, raw); file.getFD().sync();
                    }
                    if (!temp.renameTo(dest)) throw new IOException("Could not stage restore");
                    Log.i("BUGSTAR_SAVE", "cartridge restore staged");
                    endTransfer("Restore ready. Quit BUGSTAR from Options, then reopen it. The previous cartridge file is backed up.");
                } catch (Exception e) {
                    temp.delete(); endTransfer("Could not stage restore; current save unchanged");
                }
            }
        }, "bugstar-restore").start();
    }

    @Override public void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != SAVE_EXPORT && request != SAVE_IMPORT) return;
        if (!savePickerBusy || result != Activity.RESULT_OK || data == null || data.getData() == null) {
            Log.i("BUGSTAR_SAVE", "picker cancelled");
            endTransfer(null); return;
        }
        final Uri uri = data.getData();
        final byte[] snapshot = exportData;
        final String path = transferPath;
        new Thread(new Runnable() {
            @Override public void run() {
                try {
                    if (request == SAVE_EXPORT) {
                        if (snapshot == null || snapshot.length != SRAM_SIZE) throw new IOException("Missing snapshot");
                        OutputStream stream = getContentResolver().openOutputStream(uri, "w");
                        if (stream == null) throw new IOException("No output");
                        try (DataOutputStream out = new DataOutputStream(stream)) { writeSave(out, snapshot); }
                        Log.i("BUGSTAR_SAVE", "cartridge backup exported");
                        endTransfer("Cartridge backup exported");
                    } else {
                        final byte[] raw = readSave(getContentResolver().openInputStream(uri));
                        runOnUiThread(new Runnable() {
                            @Override public void run() {
                                if (isFinishing()) { endTransfer(null); return; }
                                new AlertDialog.Builder(BugstarActivity.this)
                                    .setTitle("Restore cartridge save?")
                                    .setMessage("This replaces the three cartridge slots and in-game settings on the next launch. A recovery copy of the previous file is kept. PC-only sidecar files are not included.")
                                    .setNegativeButton("Cancel", (d, which) -> endTransfer(null))
                                    .setPositiveButton("Stage restore", (d, which) -> stageRestore(path, raw))
                                    .setOnCancelListener(d -> endTransfer(null)).show();
                            }
                        });
                    }
                } catch (Exception e) {
                    endTransfer(request == SAVE_IMPORT ? "Invalid or unreadable backup; current save unchanged" : "Could not export backup");
                }
            }
        }, "bugstar-save-picker").start();
    }


    private static String readAll(File f) {
        try {
            FileInputStream in = new FileInputStream(f);
            byte[] b = new byte[(int) Math.min(f.length(), 60000)];
            int n = in.read(b);
            in.close();
            return n > 0 ? new String(b, 0, n, "UTF-8") : "";
        } catch (Throwable t) {
            return "";
        }
    }

    private static String ownLogcat() {
        StringBuilder sb = new StringBuilder();
        try {
            Process p = Runtime.getRuntime().exec(new String[] {"logcat", "-d", "-t", "600"});
            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = r.readLine()) != null) sb.append(line).append('\n');
            r.close();
        } catch (Throwable t) {
            sb.append("(could not read logcat: ").append(t).append(")\n");
        }
        return sb.toString();
    }

    // patch 35: the slow part (reading logcat, writing to Downloads) runs on its own thread, so the first screen does not wait for it.
    // What must be known before the dialog (did the last run crash, and what did it say) is read here, on the calling thread: two small files.
    private String crashText = "";

    private void collectCrashFiles() {
        File dir = getFilesDir();
        File nat = new File(dir, "native_crash.txt"), jv = new File(dir, "java_crash.txt");
        StringBuilder sb = new StringBuilder();
        if (nat.exists()) { crashedLastTime = true; sb.append("=== NATIVE CRASH (last run) ===\n").append(readAll(nat)).append('\n'); nat.delete(); }
        if (jv.exists()) { crashedLastTime = true; sb.append("=== JAVA CRASH (last run) ===\n").append(readAll(jv)).append('\n'); jv.delete(); }
        crashText = sb.toString();
    }

    private void saveReport(String crashes, String prevGameLog) {
        File dir = getFilesDir();
        StringBuilder sb = new StringBuilder();
        sb.append("BUGSTAR log, ").append(new java.util.Date()).append('\n');
        sb.append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(", Android ").append(Build.VERSION.RELEASE)
          .append(" (API ").append(Build.VERSION.SDK_INT).append("), ").append(Build.SUPPORTED_ABIS[0]).append("\n\n");
        sb.append(crashes);
        if (prevGameLog.length() > 0) sb.append("=== the game's own log of the previous run (stderr) ===\n").append(prevGameLog).append('\n');
        sb.append("=== logcat of this app ===\n").append(ownLogcat());
        byte[] data;
        try { data = sb.toString().getBytes("UTF-8"); } catch (Throwable t) { return; }
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                ContentResolver cr = getContentResolver();
                try { cr.delete(MediaStore.Downloads.EXTERNAL_CONTENT_URI, MediaStore.MediaColumns.DISPLAY_NAME + "=?", new String[] {"bugstar_log.txt"}); } catch (Throwable t) { }
                ContentValues v = new ContentValues();
                v.put(MediaStore.MediaColumns.DISPLAY_NAME, "bugstar_log.txt");
                v.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
                v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                Uri u = cr.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                OutputStream o = cr.openOutputStream(u);
                o.write(data);
                o.close();
            } else {
                FileOutputStream o = new FileOutputStream(new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "bugstar_log.txt"));
                o.write(data);
                o.close();
            }
        } catch (Throwable t) {
            try { FileOutputStream o = new FileOutputStream(new File(dir, "bugstar_report.txt")); o.write(data); o.close(); } catch (Throwable t2) { }
        }
    }

    // patch 43: the game steps at the GBA's own 59.7275 Hz from its own clock (pc_sdl.c plat_frame), never from the screen. On a 90 / 120 Hz phone the picture is shown once per game frame;
    // this tells Android that the content is a ~60 fps video-like source, so the screen can sit on 60 (or 120) Hz without judder and without burning battery. It is a hint only:
    // if it is refused (API below 30, no surface yet) nothing changes in the game's speed.
    private void hintFrameRate() {
        if (Build.VERSION.SDK_INT < 30) return;
        try {
            if (mSurface != null && mSurface.getHolder() != null) {
                Surface sf = mSurface.getHolder().getSurface();
                if (sf != null && sf.isValid()) sf.setFrameRate(59.73f, Build.VERSION.SDK_INT >= 31 ? Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE : Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
            }
        } catch (Throwable t) { }
    }

    // patch 46: low memory. Android tells the app when the phone is short of memory; the game answers by emptying its sprite cache and writing the save (pc_sdl.c, SDL_APP_LOWMEMORY).
    // onLowMemory is SDL's own (it sends the same event); onTrimMemory is the newer, earlier warning: RUNNING_LOW (10) and worse, or the app hidden (20) and worse.
    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        // UI_HIDDEN (20) alone is not a memory emergency. Avoid forcing a
        // reduced-quality session on every Home button press.
        if (level == 10 || level == 15 || level >= 60) { try { SDLActivity.nativeLowMemory(); } catch (Throwable t) { } }
    }

    private void reportSurfaceBounds() {
        View view = mSurface;
        if (view == null || view.getWidth() <= 0 || view.getHeight() <= 0) return;
        int[] xy = new int[2];
        view.getLocationOnScreen(xy);
        Log.i("BUGSTAR_VIEW", "surface left=" + xy[0] + " top=" + xy[1]
              + " w=" + view.getWidth() + " h=" + view.getHeight());
    }

    @Override
    protected void onResume() {
        super.onResume();
        hintFrameRate();
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() { @Override public void run() { hintFrameRate(); } }, 1000);      // the surface may not exist yet right at the start
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) { hintFrameRate(); reportSurfaceBounds(); }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try { collectCrashFiles(); } catch (Throwable t) { }
        final String crashes = crashText;
        final File jv = new File(getFilesDir(), "java_crash.txt");
        final Thread.UncaughtExceptionHandler old = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override public void uncaughtException(Thread t, Throwable e) {
                try {
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    FileOutputStream o = new FileOutputStream(jv);
                    o.write(("thread " + t.getName() + "\n" + sw).getBytes("UTF-8"));
                    o.close();
                } catch (Throwable t2) { }
                if (old != null) old.uncaughtException(t, e);
            }
        });
        // read the previous run's own log BEFORE the new run replaces it: the game reopens bugstar_log.txt with "w" a moment after start
        final String prevGameLog;
        { File dir = getExternalFilesDir(null); if (dir == null) dir = getFilesDir();
          File gl = new File(dir, "bugstar_log.txt"); prevGameLog = gl.exists() ? readAll(gl) : ""; }
        new Thread(new Runnable() {
            @Override public void run() {
                try { saveReport(crashes, prevGameLog); } catch (Throwable t) { }
            }
        }, "bugstar-report").start();
        super.onCreate(savedInstanceState);
        if (mSurface != null) {
            final View view = mSurface;
            view.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override public void onLayoutChange(View v, int l, int t, int r, int b,
                        int oldL, int oldT, int oldR, int oldB) {
                    reportSurfaceBounds();
                }
            });
            view.post(new Runnable() {
                @Override public void run() { reportSurfaceBounds(); }
            });
        }
        if (crashedLastTime) {
            try {
                new AlertDialog.Builder(this).setTitle("BUGSTAR crashed last time")
                    .setMessage("The crash report is being saved to Downloads/bugstar_log.txt.\n\nIn Termux: cat ~/storage/downloads/bugstar_log.txt")
                    .setPositiveButton("OK", null).show();
            } catch (Throwable t) {
                Toast.makeText(this, "BUGSTAR crashed last time: see Downloads/bugstar_log.txt", Toast.LENGTH_LONG).show();
            }
        }
    }
}
