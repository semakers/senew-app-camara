/* jpeg_cod.c — codificador JPEG base (jpeg_cod.h). Por bloque de 8×8: muestras − 128, DCT 2D separable con una tabla Q13 (la ortonormal, que es la FDCT
 * de JPEG), cuantificación con las tablas del anexo K escaladas por la calidad (la fórmula de libjpeg), zigzag y Huffman con las tablas estándar.
 * Sin coma flotante ni división por bloque salvo la cuantificación. 4:4:4: una MCU = un bloque de Y, uno de Cb y uno de Cr. */
#include "jpeg_cod.h"

static const short T[8][8] = {
    { 2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896 }, { 4017, 3406, 2276, 799, -799, -2276, -3406, -4017 },
    { 3784, 1567, -1567, -3784, -3784, -1567, 1567, 3784 }, { 3406, -799, -4017, -2276, 2276, 4017, 799, -3406 },
    { 2896, -2896, -2896, 2896, 2896, -2896, -2896, 2896 }, { 2276, -4017, 799, 3406, -3406, -799, 4017, -2276 },
    { 1567, -3784, 3784, -1567, -1567, 3784, -3784, 1567 }, { 799, -2276, 3406, -4017, 4017, -3406, 2276, -799 } };
static const unsigned char ZZ[64] = { 0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };
static const unsigned char QL[64] = { 16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99 };
static const unsigned char QC[64] = { 17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99 };
static const unsigned char DCL_B[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const unsigned char DCC_B[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const unsigned char DC_V[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const unsigned char ACL_B[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const unsigned char ACL_V[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };
static const unsigned char ACC_B[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const unsigned char ACC_V[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

typedef struct { unsigned short co[256]; unsigned char si[256]; } Huf;
typedef struct { unsigned char *d; unsigned cap, n; unsigned acc; int nb, mal; } Sal;

static void huf_hacer(Huf *h, const unsigned char *bits, const unsigned char *vals)
{
    unsigned code = 0, k = 0, l, i;
    for (l = 1; l <= 16; l++) {
        for (i = 0; i < bits[l - 1]; i++, k++) { h->co[vals[k]] = (unsigned short)code++; h->si[vals[k]] = (unsigned char)l; }
        code <<= 1;
    }
}
static void byte_(Sal *s, unsigned b) { if (s->n < s->cap) s->d[s->n++] = (unsigned char)b; else s->mal = 1; }
static void bits_(Sal *s, unsigned v, int n)                 /* n bits de v, el más alto primero; un 0xFF de datos lleva un 0x00 detrás */
{
    s->acc = (s->acc << n) | (v & ((1u << n) - 1u)); s->nb += n;
    while (s->nb >= 8) { unsigned b = (s->acc >> (s->nb - 8)) & 0xFF; byte_(s, b); if (b == 0xFF) byte_(s, 0); s->nb -= 8; }
}
static void m16_(Sal *s, unsigned v) { byte_(s, v >> 8); byte_(s, v & 0xFF); }
static int bitlen_(int v) { int n = 0; if (v < 0) v = -v; while (v) { n++; v >>= 1; } return n; }

static void fdct_(const int *in, int *out)                   /* in: 8×8 muestras − 128 (orden natural); out: los coeficientes de la FDCT de JPEG */
{
    int tmp[64], y, u, x, v;
    for (y = 0; y < 8; y++)
        for (u = 0; u < 8; u++) { int a = 0; for (x = 0; x < 8; x++) a += in[y * 8 + x] * T[u][x]; tmp[y * 8 + u] = (a + (1 << 10)) >> 11; }   /* Q2 */
    for (u = 0; u < 8; u++)
        for (v = 0; v < 8; v++) { int a = 0; for (y = 0; y < 8; y++) a += tmp[y * 8 + u] * T[v][y]; out[v * 8 + u] = (a + (1 << 14)) >> 15; }
}
static void bloque_(Sal *s, const int *mu, const unsigned char *q, int *pred, const Huf *dc, const Huf *ac)
{
    int f[64], z[64], k, run = 0, d, n;
    fdct_(mu, f);
    for (k = 0; k < 64; k++) { int c = f[ZZ[k]], qq = q[ZZ[k]]; z[k] = c >= 0 ? (c + qq / 2) / qq : -((-c + qq / 2) / qq); }
    d = z[0] - *pred; *pred = z[0];
    n = bitlen_(d);
    bits_(s, dc->co[n], dc->si[n]);
    if (n) bits_(s, (unsigned)(d < 0 ? d + (1 << n) - 1 : d), n);
    for (k = 1; k < 64; k++) {
        if (!z[k]) { run++; continue; }
        while (run > 15) { bits_(s, ac->co[0xF0], ac->si[0xF0]); run -= 16; }
        n = bitlen_(z[k]);
        bits_(s, ac->co[(run << 4) | n], ac->si[(run << 4) | n]);
        bits_(s, (unsigned)(z[k] < 0 ? z[k] + (1 << n) - 1 : z[k]), n);
        run = 0;
    }
    if (run) bits_(s, ac->co[0], ac->si[0]);                 /* EOB */
}
static void dht_(Sal *s, int clase_id, const unsigned char *bits, const unsigned char *vals, int nv)
{
    int i;
    byte_(s, (unsigned)clase_id);
    for (i = 0; i < 16; i++) byte_(s, bits[i]);
    for (i = 0; i < nv; i++) byte_(s, vals[i]);
}

unsigned jpeg_codificar(unsigned char *dst, unsigned cap, int an, int al, JpegPixel px, void *dato, int calidad)
{
    static Huf hdl, hdc, hal, hac;
    unsigned char ql[64], qc[64], p[3];
    int mu[3][64], pred[3] = { 0, 0, 0 }, esc, i, bx, by, x, y;
    Sal s;
    s.d = dst; s.cap = cap; s.n = 0; s.acc = 0; s.nb = 0; s.mal = 0;
    if (calidad < 1) calidad = 1;
    if (calidad > 100) calidad = 100;
    esc = calidad < 50 ? 5000 / calidad : 200 - 2 * calidad;
    for (i = 0; i < 64; i++) {
        int a = (QL[i] * esc + 50) / 100, b = (QC[i] * esc + 50) / 100;
        ql[i] = (unsigned char)(a < 1 ? 1 : a > 255 ? 255 : a); qc[i] = (unsigned char)(b < 1 ? 1 : b > 255 ? 255 : b);
    }
    huf_hacer(&hdl, DCL_B, DC_V); huf_hacer(&hdc, DCC_B, DC_V); huf_hacer(&hal, ACL_B, ACL_V); huf_hacer(&hac, ACC_B, ACC_V);
    m16_(&s, 0xFFD8);                                          /* SOI */
    m16_(&s, 0xFFE0); m16_(&s, 16);                            /* APP0 JFIF 1.1, sin unidades, 1:1 */
    byte_(&s, 'J'); byte_(&s, 'F'); byte_(&s, 'I'); byte_(&s, 'F'); byte_(&s, 0); byte_(&s, 1); byte_(&s, 1); byte_(&s, 0);
    m16_(&s, 1); m16_(&s, 1); byte_(&s, 0); byte_(&s, 0);
    m16_(&s, 0xFFDB); m16_(&s, 2 + 2 * 65);                    /* DQT: las dos tablas, en zigzag */
    byte_(&s, 0); for (i = 0; i < 64; i++) byte_(&s, ql[ZZ[i]]);
    byte_(&s, 1); for (i = 0; i < 64; i++) byte_(&s, qc[ZZ[i]]);
    m16_(&s, 0xFFC0); m16_(&s, 17); byte_(&s, 8); m16_(&s, (unsigned)al); m16_(&s, (unsigned)an); byte_(&s, 3);   /* SOF0 */
    byte_(&s, 1); byte_(&s, 0x11); byte_(&s, 0); byte_(&s, 2); byte_(&s, 0x11); byte_(&s, 1); byte_(&s, 3); byte_(&s, 0x11); byte_(&s, 1);
    m16_(&s, 0xFFC4); m16_(&s, 2 + 4 * 17 + 12 + 12 + 162 + 162);   /* DHT: las cuatro */
    dht_(&s, 0x00, DCL_B, DC_V, 12); dht_(&s, 0x10, ACL_B, ACL_V, 162); dht_(&s, 0x01, DCC_B, DC_V, 12); dht_(&s, 0x11, ACC_B, ACC_V, 162);
    m16_(&s, 0xFFDA); m16_(&s, 12); byte_(&s, 3);              /* SOS */
    byte_(&s, 1); byte_(&s, 0x00); byte_(&s, 2); byte_(&s, 0x11); byte_(&s, 3); byte_(&s, 0x11); byte_(&s, 0); byte_(&s, 63); byte_(&s, 0);
    for (by = 0; by < al; by += 8)
        for (bx = 0; bx < an; bx += 8) {
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x++) {                      /* fuera de la imagen (si no es múltiplo de 8): se repite el borde */
                    int xx = bx + x < an ? bx + x : an - 1, yy = by + y < al ? by + y : al - 1;
                    px(dato, xx, yy, p);
                    mu[0][y * 8 + x] = p[0] - 128; mu[1][y * 8 + x] = p[1] - 128; mu[2][y * 8 + x] = p[2] - 128;
                }
            bloque_(&s, mu[0], ql, &pred[0], &hdl, &hal);
            bloque_(&s, mu[1], qc, &pred[1], &hdc, &hac);
            bloque_(&s, mu[2], qc, &pred[2], &hdc, &hac);
            if (s.mal) return 0;
        }
    if (s.nb) bits_(&s, 0x7F, 8 - s.nb);                      /* rellenar con unos hasta el byte */
    m16_(&s, 0xFFD9);                                          /* EOI */
    return s.mal ? 0 : s.n;
}
