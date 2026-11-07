/* camara.c — «Cámara»: el visor en vivo, tomar fotos y verlas o borrarlas (API 1.9, senew-sdk/include/w5_camara.h). Decisión de diseño: JPEG de 176×144 como el original, en su carpeta y con su numeración
 * (/tarjeta/DCIM/100MSDCF/DSCnnnnn.JPG). La app CODIFICA (jpeg_cod.c, desde el YUV del sensor, girado como se ve) y el SISTEMA guarda.
 *
 *   VISOR   OK / tecla de la cámara   foto             sel. izq.  Fotos          sel. der. / Atrás  salir
 *           la imagen gira con el teléfono (W5_CAP_GIRA): de lado, la foto sale de lado también, como se ve
 *   FOTOS   OK  verla                 C  borrarla (pregunta)                    Atrás  al visor
 *   FOTO    izq. / der.  la anterior / la siguiente     C  borrarla              Atrás  a la lista
 * Con la lista o una foto abiertas la cámara se APAGA (gasta) y se vuelve a encender al volver al visor (~0,75 s). */
#include "w5.h"
#include "w5_pantalla.h"
#include "w5_musica_red.h"   /* li_pixeles (1.8) */
#include "w5_camara.h"
#include "jpeg_cod.h"

W5_APP(.nombre = "Cámara", .icono_gen = W5_IC_IMAGENES, .version = 1, .api_menor = W5_CAM_MENOR,
       .caps = W5_CAP_CAMARA | W5_CAP_IMAGENES | W5_CAP_GIRA, .pila = 32768);   /* la pila: el decodificador de im_abrir corre en ella */

#define CALIDAD 90
#define MAXF 64
static unsigned short pix[W5_CAM_ANCHO * W5_CAM_ALTO];       /* el visor, RGB565 con el byte alto primero */
static unsigned char yuyv[W5_CAM_BYTES];                     /* el cuadro de la foto, tal cual del sensor */
static unsigned char jpg[64 * 1024];
static struct {
    int abrir, giro_cam, hay, version, pedir, destello, congelado;   /* congelado: # (depuración) el visor no convierte ni pinta */
    unsigned t_aviso;
    char aviso[64];
} v;
static struct { int n, sel, im, version; char nom[MAXF][16]; unsigned tam[MAXF]; } f;
static W5Ctx visor, ctx_lista, ctx_foto;
static void cargar(void);

/* ---- la foto: del YUYV (sin girar) al JPEG girado como se ve ---- */
static void px(void *d, int x, int y, unsigned char p[3])
{
    int g = *(const int *)d, sx, sy;
    const unsigned char *q;
    switch (g) {                                              /* la inversa de lo que hace el sistema con W5_CAM_GIRO (sentido horario) */
    case 1:  sx = y;                     sy = W5_CAM_ALTO - 1 - x; break;
    case 2:  sx = W5_CAM_ANCHO - 1 - x;  sy = W5_CAM_ALTO - 1 - y; break;
    case 3:  sx = W5_CAM_ANCHO - 1 - y;  sy = x;                   break;
    default: sx = x;                     sy = y;                   break;
    }
    q = yuyv + (sy * W5_CAM_ANCHO + (sx & ~1)) * 2;
    p[0] = q[(sx & 1) * 2]; p[1] = q[1]; p[2] = q[3];          /* el YUV del sensor es el YCbCr de JFIF (el visor lo convierte con sus mismos coeficientes) */
}
static void avisar(const char *t) { unsigned i; for (i = 0; t[i] && i < sizeof v.aviso - 1; i++) v.aviso[i] = t[i]; v.aviso[i] = 0; v.t_aviso = ap_ms(); }
static void foto(void)
{
    int g = v.giro_cam, an = (g & 1) ? W5_CAM_ALTO : W5_CAM_ANCHO, al = (g & 1) ? W5_CAM_ANCHO : W5_CAM_ALTO, r;
    unsigned t0 = ap_ms(), n = jpeg_codificar(jpg, sizeof jpg, an, al, px, &g, CALIDAD), tc = ap_ms() - t0;
    char nombre[16];
    if (!n) { avisar("No se pudo codificar la foto."); return; }
    r = cam_foto_guardar(jpg, n, nombre);
    if (r == W5_OK) { avisar(ui_fmt("%s · %u KB · %u ms", nombre, (n + 512) / 1024, tc)); ap_vibrar(40); }
    else avisar(r == W5_NO_DISPONIBLE ? "No hay tarjeta donde guardarla." : "No se pudo guardar la foto.");
    ap_log(ui_fmt("camara.sen: foto %s %u B, codificar %u ms, guardar %d\n", r == W5_OK ? nombre : "-", n, tc, r));
}

/* ---- el visor ---- */
static int giro_cam(void) { return (4 - pantalla_giro()) & 3; }   /* la pantalla gira g (pantalla_giro); la imagen del sensor, al revés */
static void cada(W5Ctx c)
{
    int r;
    if (v.abrir != W5_OK) return;
    if (v.pedir) {                                            /* la foto: el SIGUIENTE cuadro entero, en YUYV */
        if (cam_cuadro(yuyv, W5_CAM_YUYV) == 1) { v.pedir = 0; foto(); v.destello = 0; v.version++; ui_sucio(ui_mi_pantalla(c)); }
        return;
    }
    if (v.congelado) return;
    v.giro_cam = giro_cam();
    r = cam_cuadro(pix, W5_CAM_RGB565 | W5_CAM_GIRO(v.giro_cam));
    if (r == 1) { v.hay = 1; v.version++; ui_sucio(ui_mi_pantalla(c)); }
    else if (v.aviso[0] && ap_ms() - v.t_aviso > 4000u) { v.aviso[0] = 0; ui_sucio(ui_mi_pantalla(c)); }
}
static void pintar(W5Lienzo l, const W5Rect *r, void *dato)
{
    int an = (v.giro_cam & 1) ? W5_CAM_ALTO : W5_CAM_ANCHO, al = (v.giro_cam & 1) ? W5_CAM_ANCHO : W5_CAM_ALTO;
    (void)dato;
    li_rect(l, r->x, r->y, r->w, r->h, 0);
    if (v.destello) li_rect(l, r->x + (r->w - an) / 2, r->y + (r->h - al) / 2, an, al, 0xFFFFFF);
    else if (v.hay) li_pixeles(l, r->x + (r->w - an) / 2, r->y + (r->h - al) / 2, an, al, pix);
}
static void abrir_fotos(void);
static int tecla(W5Ctx t, int k, void *dato)
{
    (void)dato;
    if ((k == W5K_OK || k == W5K_CAMARA) && v.abrir == W5_OK && v.hay && !v.pedir) { v.pedir = 1; v.destello = 1; v.version++; ui_sucio(ui_mi_pantalla(t)); return 1; }
    if (k == W5K_SEL_IZQ) { abrir_fotos(); return 1; }
    if (k == W5K_ALM) { v.congelado ^= 1; avisar(v.congelado ? "Visor congelado (#)" : "Visor en vivo"); ui_sucio(ui_mi_pantalla(t)); return 1; }
    if (k == W5K_OK || k == W5K_CAMARA) return 1;
    return ui_teclas_defecto(t, k);
}
static void encender(W5Ctx c)
{
    v.hay = 0; v.pedir = 0; v.destello = 0;
    v.abrir = w5_camara_hay() ? cam_abrir() : W5_NO_DISPONIBLE;
    ui_sucio(ui_mi_pantalla(c));
}
static void iniciar(W5Ctx c, void *dato) { (void)dato; visor = c; ap_retener(W5_RET_ENERGIA, 1); encender(c); ui_cada(c, 30, cada); }
static void desechar(W5Ctx c, void *dato) { (void)c; (void)dato; visor = 0; cam_cerrar(); }
static W5W construir(W5Ctx c, void *dato)
{
    W5CamInfo i;
    const char *linea;
    (void)c; (void)dato;
    memset(&i, 0, sizeof i);
    cam_info(&i);
    linea = v.abrir == W5_NO_DISPONIBLE ? "Este teléfono no tiene cámara."
          : v.abrir != W5_OK ? ui_fmt("La cámara no responde (%d).", i.error)
          : v.pedir ? "Tomando la foto…"
          : v.aviso[0] ? v.aviso
          : i.estado == W5_CAM_ASENTANDO || !v.hay ? "Encendiendo la cámara…" : "OK: tomar una foto";
    return Teclas(tecla, ui_andamio("Cámara", Columna(A_CRUZADO(W5P_ESTIRAR), HIJOS(
        Expandido(Pintura(pintar, A_VERSION(v.version), A_AN(W5_INF), A_AL(W5_INF))),
        RellenoXY(4, 2, Texto(linea, A_MARCAS(W5_F_ESTILO(W5_T_PEQUENA)))))),
        "Fotos", "Atrás"));
}

/* ---- las fotos: las DSCnnnnn.JPG de la carpeta, la más nueva arriba ---- */
static int es_foto(const char *n)
{
    int i;
    if (memcmp(n, "DSC", 3) || memcmp(n + 8, ".JPG", 5)) return 0;
    for (i = 3; i < 8; i++) if (n[i] < '0' || n[i] > '9') return 0;
    return 1;
}
static void listar(void)
{
    static W5Ent e[16];
    unsigned desde = 0;
    int k, i, j;
    f.n = 0;
    for (;;) {
        k = fs_listar(W5_CAM_CARPETA, desde, e, 16);
        for (i = 0; i < k && f.n < MAXF; i++)
            if (!e[i].es_dir && es_foto(e[i].nombre)) { memcpy(f.nom[f.n], e[i].nombre, 13); f.tam[f.n] = e[i].tam; f.n++; }
        if (k < 16) break;
        desde += 16;
    }
    for (i = 1; i < f.n; i++)                                 /* la más nueva (número más alto) arriba */
        for (j = i; j > 0 && memcmp(f.nom[j], f.nom[j - 1], 13) > 0; j--) {
            char t[16]; unsigned s = f.tam[j];
            memcpy(t, f.nom[j], 16); memcpy(f.nom[j], f.nom[j - 1], 16); memcpy(f.nom[j - 1], t, 16);
            f.tam[j] = f.tam[j - 1]; f.tam[j - 1] = s;
        }
    if (f.sel >= f.n) f.sel = f.n ? f.n - 1 : 0;
}
static void borrar_si(void *dato)
{
    int i = (int)(size_t)dato, r;
    if (i < 0 || i >= f.n) return;
    r = cam_foto_borrar(f.nom[i]);
    ui_aviso_breve(r == W5_OK ? ui_fmt("%s borrada", f.nom[i]) : "No se pudo borrar.");
    listar(); f.version++;
    if (ctx_foto) { if (!f.n) ui_sacar(); else { cargar(); ui_sucio(ui_mi_pantalla(ctx_foto)); } }
    if (ctx_lista) ui_sucio(ui_mi_pantalla(ctx_lista));
}
static void preguntar_borrar(int i)
{
    if (i < 0 || i >= f.n) return;
    ui_confirmar("Borrar la foto", ui_fmt("¿Borrar %s de la tarjeta? No se puede deshacer.", f.nom[i]), borrar_si, (void *)(size_t)i);
}

static void abrir_una(void);
static W5W fila(W5Ctx c, int i, int sl, void *dato)
{
    (void)c; (void)dato;
    if (i < 0 || i >= f.n) return ui_fila("", 0, sl);
    return ui_fila(f.nom[i], ui_fmt("%u KB", (f.tam[i] + 512) / 1024), sl);
}
static void activar(W5Ctx c, int i, void *dato) { (void)c; (void)dato; f.sel = i; abrir_una(); }
static int tecla_lista(W5Ctx t, int k, void *dato)
{
    (void)dato;
    if (k == W5K_C) { preguntar_borrar(f.sel); ui_sucio(ui_mi_pantalla(t)); return 1; }
    return ui_teclas_defecto(t, k);
}
static void iniciar_lista(W5Ctx c, void *dato) { (void)dato; ctx_lista = c; cam_cerrar(); listar(); }
static void desechar_lista(W5Ctx c, void *dato) { (void)c; (void)dato; ctx_lista = 0; if (visor) encender(visor); }   /* de vuelta al visor: la cámara otra vez */
static W5W construir_lista(W5Ctx c, void *dato)
{
    (void)c; (void)dato;
    return Teclas(tecla_lista, ui_andamio(f.n ? ui_fmt("Fotos (%d)", f.n) : "Fotos",
        f.n ? ListaPerezosa(f.n, fila, A_AL_ACTIVAR(activar), A_SEL(&f.sel))
            : RellenoXY(8, 8, Parrafo("Aún no hay fotos. En el visor, OK toma una (se guardan en la tarjeta, en DCIM/100MSDCF).", A_COLOR(ui_color(W5_COL_TENUE)))),
        "Ver", "Atrás"));
}
static void abrir_fotos(void) { ui_empujar(ui_pantalla(construir_lista, iniciar_lista, desechar_lista, 0)); }

/* ---- una foto ---- */
static void cargar(void)
{
    if (f.im > 0) im_cerrar(f.im);
    f.im = f.sel >= 0 && f.sel < f.n ? im_abrir(ui_fmt("%s/%s", W5_CAM_CARPETA, f.nom[f.sel])) : 0;
    f.version++;
}
static void pintar_foto(W5Lienzo l, const W5Rect *r, void *dato)
{
    W5ImInfo i;
    (void)dato;
    li_rect(l, r->x, r->y, r->w, r->h, 0);
    if (f.im > 0 && im_info(f.im, &i) == W5_OK) im_pintar(l, f.im, r->x + (r->w - i.w) / 2, r->y + (r->h - i.h) / 2);
}
static int tecla_foto(W5Ctx t, int k, void *dato)
{
    (void)dato;
    if ((k == W5K_IZQ || k == W5K_DER) && f.n) { f.sel = (f.sel + (k == W5K_DER ? 1 : f.n - 1)) % f.n; cargar(); ui_sucio(ui_mi_pantalla(t)); return 1; }
    if (k == W5K_C) { preguntar_borrar(f.sel); return 1; }
    if (k == W5K_OK || k == W5K_SEL_IZQ) return 1;   /* (28-oct) nada que hacer, y dejarlas al defecto re-entra aquí (ver grabadora.c) */
    return ui_teclas_defecto(t, k);
}
static void iniciar_foto(W5Ctx c, void *dato) { (void)dato; ctx_foto = c; f.im = 0; cargar(); }
static void desechar_foto(W5Ctx c, void *dato) { (void)c; (void)dato; ctx_foto = 0; if (f.im > 0) im_cerrar(f.im); f.im = 0; }
static W5W construir_foto(W5Ctx c, void *dato)
{
    (void)c; (void)dato;
    return Teclas(tecla_foto, ui_andamio(f.sel < f.n ? f.nom[f.sel] : "Foto", Columna(A_CRUZADO(W5P_ESTIRAR), HIJOS(
        Expandido(Pintura(pintar_foto, A_VERSION(f.version), A_AN(W5_INF), A_AL(W5_INF))),
        RellenoXY(4, 2, Texto(f.im > 0 ? ui_fmt("%d de %d · izq./der. · C borra", f.sel + 1, f.n) : "No se puede abrir.", A_MARCAS(W5_F_ESTILO(W5_T_PEQUENA)))))),
        "", "Atrás"));
}
static void abrir_una(void) { ui_empujar(ui_pantalla(construir_foto, iniciar_foto, desechar_foto, 0)); }

W5Pal app_suceso(int suceso, W5Pal a, W5Pal b)
{
    (void)a; (void)b;
    if (suceso == W5_ABRIR) return (W5Pal)ui_pantalla(construir, iniciar, desechar, 0);
    return 0;
}
