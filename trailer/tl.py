# tl.py: the trailer's timeline, shared by the music and the edit (seconds; 120 BPM, a bar is 2 s)
BPM=120; BEAT=60/BPM; BAR=4*BEAT
def bt(bar, beat=0): return bar*BAR+beat*BEAT
T_INTRO=0.0        # the quiet start: the little bug
T_RISER=12.0       # the riser into the dungeons
T_DUNGEON=16.0     # drums in: dungeons and bosses
T_BOSS=32.0        # the boss riff
T_SPACE=40.0       # space: the ship, the fleet
T_BEAST=56.0       # the Technobeast
T_WORLDS=64.0      # the worlds montage: universes, weird worlds, hell worlds
T_STOP=88.0        # the record stop: WAIT. KART RACING?
T_KART=90.0        # chiptune kart racing
T_MAKERS=104.0     # back to epic: makers, HOW BIG IS IT?
T_CLIMAX=128.0     # the hero fanfare, the fastest cuts
T_HIT=152.0        # the last hit, then silence: the small cartridge joke
T_LAUNCH=156.0     # sunrise over the planet
T_IGNITE=160.0     # engines light
T_LIFT=162.0       # lift-off
T_FLYBY=171.3      # the ship fills the lens
T_FLASH=171.5      # white
T_TITLE=172.0      # BUGSTAR
T_END=177.5
