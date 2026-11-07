/* jpeg_cod.h — un codificador JPEG base (JFIF, 8 bits, 4:4:4, tablas de Huffman del anexo K) pequeño y ENTERO (el ARM926 del W580 no tiene FPU).
 * Para la app «Cámara» (senew-app-camara): codifica desde el YUV del sensor sin pasar por RGB. Portable: se puede probar en un PC. */
#ifndef JPEG_COD_H
#define JPEG_COD_H
/* ycc: el píxel (x, y) en Y Cb Cr (JFIF, 0..255) */
typedef void (*JpegPixel)(void *dato, int x, int y, unsigned char ycc[3]);
/* calidad 1..100 (la de libjpeg). Devuelve los bytes escritos en dst, o 0 si no cupo. */
unsigned jpeg_codificar(unsigned char *dst, unsigned cap, int ancho, int alto, JpegPixel px, void *dato, int calidad);
#endif
