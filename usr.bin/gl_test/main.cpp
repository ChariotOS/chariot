#include <GL/glu.h>
#include <GL/osmesa.h>


int main(int argc, char** argv) {
  OSMesaContext om = OSMesaCreateContext(OSMESA_BGRA, NULL);
  OSMesaDestroyContext(om);
  return 0;
}
