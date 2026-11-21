#!/usr/bin/env python3
"""Exercise Bugstar's real low-speed kart steering helpers under UBSan.

The C functions are extracted from kart.inc instead of being reimplemented.
This tests fixed-tick math and source integration, not a handheld playthrough.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"kart.inc").read_text()


def extract(name):
    prefix=f"static inline int {name}("
    start=source.index(prefix)
    opening=source.index("{",start)
    depth=0
    for i in range(opening,len(source)):
        if source[i]=="{":
            depth+=1
        elif source[i]=="}":
            depth-=1
            if depth==0:
                return source[start:i+1]
    raise AssertionError("Missing closing brace: "+name)


functions=[extract(name) for name in
           ("kart_turn_rate","kart_turn_direction","kart_recovery_slow")]
assert "tr=kart_turn_rate(tr,kv,vmax);" in source, "steering helper is not active in kart_step"
assert "int steer=kart_turn_direction(s,kv); khd=(khd+steer*tr)&1023;" in source, (
    "reverse steering is not connected to the real heading update"
)
assert "if(kart_recovery_slow(kv) && !kroad(c) && !kstall)" in source, (
    "rescue still mistakes fast reverse for a stalled kart"
)
assert 'show_msg("STUCK? DOWN: REVERSE OR WAIT")' in source, (
    "player should get one clear recovery hint before teleport"
)

harness=r"""
#include <stdint.h>
#include <stdio.h>
typedef int32_t s32;
#define K_VBASE 0x3200
#define CHECK(e) do { if(!(e)){ fprintf(stderr,"Kart control failed at line %d: %s\n", __LINE__, #e); return 1; } } while(0)
int main(void){
    const s32 max=12800;
    CHECK(kart_turn_rate(7,0,max)==0);             // no stationary pirouette
    CHECK(kart_turn_rate(7,max/64,max)==0);       // very tiny speed still has little effect
    CHECK(kart_turn_rate(7,max/32,max)==2);       // speed threshold gives usable turning
    CHECK(kart_turn_rate(5,max/16,max)==2);       // slippery/low-grip speed floor
    CHECK(kart_turn_rate(7,max/4,max)==5);        // above slow speeds, preserve original curve
    CHECK(kart_turn_rate(7,max/2,max)==7);
    CHECK(kart_turn_rate(7,max,max)==7);
    CHECK(kart_turn_rate(7,-max/16,max)==2);      // reverse has the same turn magnitude
    CHECK(kart_turn_direction(1,max/3)==1);
    CHECK(kart_turn_direction(-1,max/3)==-1);
    CHECK(kart_turn_direction(1,-max/3)==-1);    // steering direction reverses
    CHECK(kart_turn_direction(-1,-max/3)==1);
    CHECK(kart_recovery_slow(0));
    CHECK(kart_recovery_slow(K_VBASE/15));
    CHECK(kart_recovery_slow(-K_VBASE/15));
    CHECK(!kart_recovery_slow(K_VBASE/3));        // fast forward and reverse aren't stuck
    CHECK(!kart_recovery_slow(-K_VBASE/3));
    puts("Kart controls: low speed, reverse steering and signed recovery PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-kart-controls-") as d:
    src=Path(d)/"steering_test.c"
    exe=Path(d)/"steering_test"
    src.write_text(harness[:harness.index("int main(void){")] +
                   "\n".join(functions) + "\n" +
                   harness[harness.index("int main(void){"):])
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
