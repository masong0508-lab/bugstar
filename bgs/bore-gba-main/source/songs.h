// songs.h - THE JUKEBOX SONG LIST. Order here = the "IN ORDER" order. Up to 32 songs.
//
//   SONG_XM(id,"NAME","file")    a tracker song (.xm). id = any unique C name, file = the .xm in tools/. After adding one, run
//                                python3 tools/xm2gba.py   to rebuild source/musicdata.h (the title music, THE DIPPER MAN, is one too)
//   SONG_ADP(id,"NAME","file")   a streamed ADPCM song. id = any unique C name, NAME = capitals / digits / spaces only
//                                (the on-screen font has no punctuation), file = a .adp made by tools/encode_song.py
//
// Easiest way to add a song:   python3 tools/encode_song.py "my song.mp3"
// It writes source/music/<id>.adp and appends the SONG_ADP line below for you.
//
// Song spec: 4-bit IMA-ADPCM, mono, 18157 Hz (the game's music rate), about 9 KB per second of music.
// When this list changes size the saved shuffle is re-rolled automatically (next boot).

SONG_XM(the_dipper_man,"THE DIPPER MAN","tools/the_dipper_man.xm")

// ---- PLACEHOLDERS (short test tunes so shuffle / select can be heard now) ----
// Hidden in the jukebox unless the title-screen code UP UP DOWN DOWN LEFT LEFT RIGHT B A START is entered (any song named PLACEHOLDER...).
// Delete these 3 lines and the 3 files in source/music/ when you add your real songs.
SONG_ADP(placeholder_a,"PLACEHOLDER A","source/music/placeholder_a.adp")
SONG_ADP(placeholder_b,"PLACEHOLDER B","source/music/placeholder_b.adp")
SONG_ADP(placeholder_c,"PLACEHOLDER C","source/music/placeholder_c.adp")

// ---- songs added by tools/encode_song.py go below this line ----
SONG_XM(earth_and_the_space_citizens,"EARTH AND THE SPACE CITIZENS","tools/earth_and_the_space_citizens.xm")
SONG_XM(amiga_music,"AMIGA MUSIC","tools/amiga_music.xm")
SONG_XM(emergency_dance_floor,"EMERGENCY ON THE DANCE FLOOR","tools/emergency_dance_floor.xm")
SONG_XM(tree_swaying_action,"TREE SWAYING ACTION","tools/tree_swaying_action.xm")
SONG_XM(meltdown_in_mars_house,"MELTDOWN IN MARS 90S HOUSE","tools/meltdown_in_mars_house.xm")
SONG_XM(worthless_clouds,"WORTHLESS CLOUDS","tools/worthless_clouds.xm")
