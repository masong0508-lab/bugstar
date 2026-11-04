package org.bugstar.app;

// patch 34: SDL's activity plus a crash report you can read without adb or the data folder. (patch 35: the report is written on a background thread.)
// On every start it saves the app's own recent log (the previous run's too) and any crash notes to Downloads/bugstar_log.txt,
// and it tells you on screen when the last run crashed. (An app can always read its own logcat; nothing else is needed.)

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.widget.Toast;

import java.io.BufferedReader;
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
        { File gl = new File(getExternalFilesDir(null), "bugstar_log.txt"); prevGameLog = gl.exists() ? readAll(gl) : ""; }
        new Thread(new Runnable() {
            @Override public void run() {
                try { saveReport(crashes, prevGameLog); } catch (Throwable t) { }
            }
        }, "bugstar-report").start();
        super.onCreate(savedInstanceState);
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
