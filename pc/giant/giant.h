// giant.h: the GIANT NUMBERS of the BUGSTAR PC port (plain C, no game code, no libraries). See pc/giant/README.md.
//   * Two numbers with more than 1,000,000,000 decimal digits, every digit really there, never held whole in memory:
//       GI_POW   2^(2^32): the number of different things 512 MiB (2^32 bits) of memory can hold. 1,292,913,987 digits.
//                Computed once, exactly, by pc/giant/mkpow.py into a file (646 MB, packed decimal); this code seeks in that file.
//       GI_COUNT THE COUNTING NUMBER: 1 2 3 4 ... written one after another up to 123,456,790. 1,000,000,008 digits.
//                Needs no file: digit N is worked out on the spot (Champernowne's constant, cut after 123,456,790).
//   * Names: the Conway-Wechsler -illion names (as the GBA game's illion()), English number words, and the parts of a name.
//   * BUGSTARILLION v3: a number whose complete written-out name (about 6 x 10^18 letters) is a stream you can seek in and export.
#ifndef GIANT_H
#define GIANT_H
typedef unsigned long long gu64;

#define GI_POW 0
#define GI_COUNT 1
#define GI_N 2
#define GI_POW_E 32                        // 2^(2^GI_POW_E)
#define GI_POW_DIGITS 1292913987ull        // its exact digit count
#define GI_COUNT_LAST 123456790ull         // the counting number ends with this
#define GI_COUNT_DIGITS 1000000008ull

// ---- where the files are kept: $BUGSTAR_GIANT_DIR, else <program folder>/giant/cache, else ./giant/cache, else <save folder>/giant
void gi_set_dir(const char *dir);          // use this folder (null: look in the usual places)
const char *gi_dir(void);                  // the folder in use ("" if none found yet)
const char *gi_file(int which);            // the file name inside it
int gi_open(int which);                    // 0 = ready, 1 = file not there (build it with pc/giant/mkpow.py), 2 = file is wrong
int gi_ready(int which);
gu64 gi_digits(int which);                 // exact digit count
const char *gi_title(int which);
// n digits starting at digit number pos (0 = the first, the leftmost). Bytes are ASCII '0'..'9'. Past the end: '\0'. Returns how many real digits.
int gi_read(int which, gu64 pos, int n, char *out);

// ---- naming (Conway-Wechsler; the same rules as the game's gcw_prefix, see names.txt in pc/giant)
int gcw_prefix(int v, char *out);           // 0..999 -> Latin prefix as the game builds it: 357 -> TRESQUINQUAGINTASEPTINGENTI (not yet cut). returns length
int gcw_tok(int v, int last, char *out);    // one chunk of an -illion name: last I or A cut, then ILLI (or ILLION when last). returns length
int gcw_toklen(int v);                      // length of the ILLI form (the ILLION form is 2 longer)
// the name of the m-illion (10^(3m+3)); m is a decimal string of any length. Writes at most cap-1 letters; returns the full length (may be > cap-1).
gu64 gcw_place(const char *m, char *out, int cap);
gu64 gcw_place_len(const char *m);
// the pieces a chunk's prefix is built from, with what each means: p[i] = text, d[i] = meaning. returns the number of pieces (1 to 3)
int gcw_parts(int v, char p[3][24], char d[3][28]);
int gen_chunk(int v, char *out);            // 0..999 in English: "THREE HUNDRED FIFTY-SEVEN" (999 -> NINE HUNDRED NINETY-NINE). returns length
int gen_chunklen(int v);
void gen_u64(gu64 v, char *out, int cap);   // any 64-bit number in English short scale words (quintillion at most)
// ---- decimal strings (a number as ASCII digits, no sign)
void gds_div3(const char *s, char *q, int *r);      // q = s / 3 (no leading zeros), r = s mod 3
void gds_sub1(const char *s, char *o);              // o = s - 1 (s >= 1)
int gds_cmp(const char *a, const char *b);
void gds_comma(const char *s, char *o, int cap);    // 1234567 -> 1,234,567

// ---- exporting a whole giant number as a text file (100 digits to a line); progress() returns nonzero to cancel
typedef int (*gi_progress_fn)(gu64 done, gu64 total, void *ud);
int gi_export(int which, const char *path, gi_progress_fn pr, void *ud, char sha_hex[65], gu64 *bytes);   // 0 ok, 1 cancelled, 2 write error, 3 not ready
void gsha256_buf(const unsigned char *d, gu64 n, unsigned char out[32]);
typedef struct { unsigned int h[8]; unsigned char buf[64]; gu64 len; int nb; } GiSha;
void gsha_init(GiSha *s); void gsha_add(GiSha *s, const void *d, gu64 n); void gsha_end(GiSha *s, char hex[65]);

// ---- BUGSTARILLION v3: V3 = sum over j = 1..L of c(j) x 1000^(n(j)+1), L = 1,292,913,987 = the digits of 2^(2^32),
//      n(j) = the number made by the first j digits of 2^(2^32), c(j) = 100 + hash(j) mod 900. Each term is said "COEFFICIENT n(j)-ILLION".
//      The stream is the whole name, biggest term first: "NNN WORDS NAME, NNN WORDS NAME, ... ." (separator ", ", the last ends with ".")
typedef struct {
    gu64 j;            // the term (1..L) the letter belongs to
    gu64 start;        // stream position where this term's text starts
    gu64 len;          // its length (with the ", " after it; the last term has one letter less)
    gu64 off;          // the letter's place inside the term
    int coef;          // c(j)
    int a;             // digits in the first chunk of n(j): 1, 2 or 3
    gu64 nchunks;      // chunks of n(j) = 1 + (j-a)/3
    gu64 placelen;     // letters in the -illion name of this term
    int seg;           // 0 coefficient words, 1 space, 2 the -illion name, 3 the separator
    gu64 chunk;        // when seg == 2: which chunk (0 = first) holds the letter
} V3Info;
int gv3_open(void);                         // needs GI_POW ready; builds (about 10 s) or loads the index file. 0 ok
int gv3_ready(void);
gu64 gv3_parts(void);                       // L
gu64 gv3_chars(void);                       // letters in the whole name
int gv3_coef(gu64 j);
int gv3_read(gu64 pos, int n, char *out);   // n letters of the name from letter pos. returns how many (less at the end)
int gv3_info(gu64 pos, V3Info *vi);         // 0 ok
gu64 gv3_term_len(gu64 j);                  // letters in term j
gu64 gv3_term_start(gu64 j);                // stream position of term j
int gv3_chunk(gu64 j, gu64 ci);             // chunk ci (0 first) of n(j), 0..999
int gv3_export(const char *path, gu64 start, gu64 n, gi_progress_fn pr, void *ud, char sha_hex[65]);
// ---- the older record, for the comparison: v2's numbers (from facts.py, checked by tests)
#endif
