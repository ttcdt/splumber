/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* Space Plumber 0.9.2 funcionó en Linux + svgalib el 05/08/1997 */
/* Space Plumber 0.9.3 funcionó (toscamente) el laberinto el 14/08/1997 */
/* Space Plumber 0.9.4 funcionaron el menú, los forks, broken_light,
   _writable_dir y el shift izquierdo el 21/08/1997 */
/* Space Plumber 0.9.5 añadido la bifurcación en K, terminado el
   motor de laberintos aleatorios 28/08/1997 */
/* Space Plumber 1.0.1 el 17/09/1997 */
/* Space Plumber 1.0.2 el 19/11/1997 para Stratos */
/* Space Plumber 1.0.3 añadido mirar arriba y abajo y _wave_water */
/* Space Plumber 1.0.4 versión X11 y otros cambios estéticos
   en el código fuente */
/* Space Plumber 1.0.4i nuevo el parámetro intro */
/* Space Plumber 1.0.5 con soporte de joystick 21/03/1998 */
/* Space Plumber 1.0.5a ventana X11 a doble tamaño 28/07/2000 */
/* Space Plumber 1.0.5b funciona a color de 16 bits y esquivado
   (no arreglado) el error del SIGSEGV 31/07/2000 */
/* Space Plumber 1.0.5c posicionamiento de ventanas correcto 01/08/2000 */
/* Space Plumber 1.0.5d funciona en color 24 y 32 bits 02/08/2000 */

#define SCREEN_X_SIZE 320
#define SCREEN_Y_SIZE 200
#define SCREEN_HALF_Y_SIZE 100

#define TEXTURE_X_SIZE 128
#define TEXTURE_Y_SIZE 128
#define TEXTURE_X_MASK 127
#define TEXTURE_Y_MASK 127
#define TEXTURE_SIZE   (TEXTURE_X_SIZE * TEXTURE_Y_SIZE)
#define TEXTURE_MASK   (TEXTURE_SIZE - 1)

#define BLOCK_BITS 7

#define BLOCK_SIZE (1 << BLOCK_BITS)
#define BLOCK_MASK (BLOCK_SIZE - 1)


typedef unsigned char sp_texture;
