#include <ck/timer.h>
#include <math.h>
#include <stdio.h>
#include <ui/application.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include <GL/osmesa.h>


#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"


#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })




typedef struct {
  float x;
  float y;
  float z;
} vector3_t;

typedef struct {
  vector3_t position;
  vector3_t normal;
  vector3_t texture;
} vertex_t;


typedef struct {
  int count;
  vertex_t* vertices;
} face_t;

typedef struct {
  int facec;
  face_t* facev;
} obj3d_t;


static int count(char* s, char c) {
  int i = 0;
  for (i = 0; s[i]; s[i] == c ? i++ : *s++)
    ;
  return i;
}

obj3d_t obj3dopen(const char* filename) {
  obj3d_t object = {};

  FILE* file;
  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "[object.c] Error loading object file '%s'\n", filename);
  }

  int vcount = 0;
  int fcount = 0;
  int vncount = 0;
  int vtcount = 0;
  int lines = 0;
  // We need to figure out everything we need, so we
  // read over the file, line by line frist.
  char line[256];
  while (fgets(line, sizeof(line), file)) {
    lines++;  // The number of lines increases every line, of course

    // Now we determine what the line is
    if (strncmp("v ", line, 2) == 0) vcount++;
    if (strncmp("vn ", line, 2) == 0) vncount++;
    if (strncmp("vt ", line, 2) == 0) vtcount++;
    if (strncmp("f ", line, 2) == 0) fcount++;
  }

  auto* vertices = new vector3_t[vcount];
  auto* normals = new vector3_t[vncount];
  auto* textures = new vector3_t[vtcount];
  face_t* faces = new face_t[fcount];

  // Current indexes in the different arrays of data.
  int vi = 0;
  int vni = 0;
  int vti = 0;
  int fi = 0;
  // Now that we have the data allocated, we read the file again.
  rewind(file);
  while (fgets(line, sizeof(line), file)) {
    if (strncmp("v ", line, 2) == 0) {
      float x, y, z;
      // printf("%s", line);
      sscanf(line, "v%f%f%f", &x, &y, &z);
      vertices[vi].x = x;
      vertices[vi].y = y;
      vertices[vi].z = z;
      vi++;
      continue;
    }

    if (strncmp("vn ", line, 2) == 0) {
      float x, y, z;
      // printf("%s", line);
      sscanf(line, "vn%f%f%f", &x, &y, &z);
      normals[vni].x = x;
      normals[vni].y = y;
      normals[vni].z = z;
      vni++;
      continue;
    }

    if (strncmp("vt ", line, 2) == 0) {
      float x, y, z;
      // printf("%s", line);
      sscanf(line, "vn%f%f%f", &x, &y, &z);
      textures[vti].x = x;
      textures[vti].y = y;
      textures[vti].z = z;
      vti++;
      continue;
    }


    if (strncmp("f ", line, 2) == 0) {
      face_t face;
      if (count(line, ' ') == 4) {
        // Quad
        face.count = 4;
        face.vertices = new vertex_t[face.count];
        int v0, n0;
        int v1, n1;
        int v2, n2;
        int v3, n3;
        // Scan into the data, with this ugly, terrible, disgraceful sscanf...
        sscanf(line, "f %d//%d %d//%d %d//%d %d//%d", &v0, &n0, &v1, &n1, &v2, &n2, &v3, &n3);
        face.vertices[0].position = vertices[v0 - 1];
        face.vertices[0].normal = normals[n0 - 1];

        face.vertices[1].position = vertices[v1 - 1];
        face.vertices[1].normal = normals[n1 - 1];

        face.vertices[2].position = vertices[v2 - 1];
        face.vertices[2].normal = normals[n2 - 1];

        face.vertices[3].position = vertices[v3 - 1];
        face.vertices[3].normal = normals[n3 - 1];
      } else {
        // Triangle
        face.count = 3;
        face.vertices = new vertex_t[face.count];
        int v0, n0;
        int v1, n1;
        int v2, n2;
        // Scan into the data, with this ugly, terrible, disgraceful sscanf...
        sscanf(line, "f %d//%d %d//%d %d//%d", &v0, &n0, &v1, &n1, &v2, &n2);
        face.vertices[0].position = vertices[v0 - 1];
        face.vertices[0].normal = normals[n0 - 1];

        face.vertices[1].position = vertices[v1 - 1];
        face.vertices[1].normal = normals[n1 - 1];

        face.vertices[2].position = vertices[v2 - 1];
        face.vertices[2].normal = normals[n2 - 1];
      }
      faces[fi++] = face;
      continue;
    }

    // printf("UNHANDLED: %s", line);
  }

  object.facec = fcount;
  object.facev = faces;

  free(vertices);
  free(normals);
  free(textures);
  fclose(file);

  return object;
}




class glpainter : public ui::view {
  OSMesaContext om;
  bool initialized = false;
  float xrot = 100.0f;
  float yrot = -100.0f;

  // float xdiff = 100.0f;
  // float ydiff = 100.0f;

  float tra_x = 0.0f;
  float tra_y = 0.0f;
  float tra_z = 0.0f;

  float grow_shrink = 70.0f;
  float resize_f = 1.0f;

  ck::ref<ck::timer> compose_timer;

  obj3d_t mesh = obj3dopen("/usr/res/obj/teapot.obj");

 public:
  glpainter(void) { om = OSMesaCreateContext(OSMESA_BGRA, NULL); }

  ~glpainter(void) { OSMesaDestroyContext(om); }


  void reshape(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);

    gluPerspective(grow_shrink, resize_f * w / h, resize_f, 100 * resize_f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }

  void tick() {
    xrot += 0.3f;
    yrot += 0.4f;
    repaint();
  }



  // This will identify our vertex buffer
  unsigned int buffer;
  unsigned int shader;

  static int compile_shader(const ck::string& source, unsigned type) {
    unsigned id = glCreateShader(type);
    const char* src = source.get();
    // printf("compile shader. id=%d: \n%s\n", id, source.get());

    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);

    if (result == GL_FALSE) {
      int length = 0;
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
      char* msg = new char[length];

      glGetShaderInfoLog(id, length, &length, msg);
      printf("failed to compile shader: '%s'\n", msg);
      delete[] msg;
      glDeleteShader(id);
      // return a failed state
      return 0;
    }

    return id;
  }

  static int compile_shader(const ck::string& vertexShader, const ck::string& fragmentShader) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compile_shader(vertexShader, GL_VERTEX_SHADER);
    unsigned int fs = compile_shader(fragmentShader, GL_FRAGMENT_SHADER);

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);
    glValidateProgram(program);

    // now we have linked, we don't need the original shaders :)
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
  }


  // just some positions for a triangle :)
  float positions[6] = {
      -0.5f, -0.5f,  // [0]
      0.0f,  1.0f,   // [1]
      0.5f,  -0.5f,  // [2]
  };

  int initgl(void) {
    if (initialized == true) return 0;

    compose_timer = ck::timer::make_interval(1000 / 30, [this] { this->tick(); });
    initialized = true;
    auto* win = window();
    auto& bmp = win->bmp();

    if (!OSMesaMakeCurrent(om, bmp.pixels(), GL_UNSIGNED_BYTE, bmp.width(), bmp.height())) return 1;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
		return 0;

#if 0

    // create a buffer
    glGenBuffers(1, &buffer);
    // bind to the buffer we are using. This specifies that we are
    // about to work on this specific buffer
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    // and put the positions in the buffer :)
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    // But, opengl doesn't understand the structure we just passed in! It doesn't know
    // how it is layed out. Is it a bunch of vec3 items? vec2? !!!

    // This guy sets that up!
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, positions);
#endif


    ck::string vertexShader = R"SHD(
        #version 120
				attribute vec4 position;

				uniform mat4 gl_ModelViewMatrix;
				uniform mat4 gl_ProjectionMatrix;

        void main() {
						gl_Position = ftransform(); // gl_ProjectionMatrix * gl_ModelViewMatrix * position;
        }
		)SHD";


    ck::string fragmentShader = R"SHD(
        #version 120
        void main() {
					gl_FragColor = vec4(0.4, 0.4, 0.8, 1.0);
        }
		)SHD";

    shader = compile_shader(vertexShader, fragmentShader);

    return 0;
  }




  virtual void paint_event(void) override {
    glViewport(0, 0, width(), height());

    glMatrixMode(GL_PROJECTION);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    glEnable(GL_MULTISAMPLE);

    gluPerspective(64.0, width() / (double)height(), 0.01, 100.0);

    gluLookAt(0.0f, 0.0f, 10.0f,  // eye pos
              0.0f, 0.0f, 0.0f,   // center pos
              0.0f, 1.0f, 0.0f    // up pos?
    );
    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);

    glColor3f(1.0, 1.0, 1.0);

    for (int i = 0; i < mesh.facec; i++) {
      face_t& face = mesh.facev[i];
      if (face.count == 3) {
        glBegin(GL_TRIANGLES);
        for (int v = 0; v < face.count; v++) {
          glVertex3f(face.vertices[v].position.x, face.vertices[v].position.y, face.vertices[v].position.z);
        }
        glEnd();
      } else {
        glBegin(GL_QUADS);
        for (int v = 0; v < face.count; v++) {
          glVertex3f(face.vertices[v].position.x, face.vertices[v].position.y, face.vertices[v].position.z);
        }
        glEnd();
      }
    }

    // gluLookAt(0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    // glRotatef(yrot, 0.0f, 1.0f, 0.0f);
#if 0
    glUseProgram(shader);

		for (int i = 0; i < mesh.facec; i++) {
			auto &face = mesh.facev[i];
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, face.vert);
glEnableVertexAttribArray(0);
glDrawArrays(GL_TRIANGLES, 0, 3);


		}
#endif

    glFlush();

    invalidate();
  }

  void drawBox() {
    glTranslatef(tra_x, tra_y, tra_z);

    glBegin(GL_QUADS);

    glColor3f(1.0f, 0.0f, 0.0f);
    // FRONT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    // BACK
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);

    glColor3f(0.0f, 1.0f, 0.0f);
    // LEFT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    // RIGHT
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glColor3f(0.0f, 0.0f, 1.0f);
    // TOP
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    // BOTTOM
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glEnd();  // GL_QUADS
  }
};

int main(int argc, char** argv) {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("OpenGL Cube!", 640, 480);
  auto& vw = win->set_view<glpainter>();
  vw.initgl();

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
