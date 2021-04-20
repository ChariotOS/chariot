#include <chariot/fs/magicfd.h>
#include <ck/rand.h>
#include <ck/timer.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <gfx/image.h>
#include <math.h>
#include <pthread.h>
#include <sys/mman.h>
#include <ui/application.h>
#include <ui/color.h>
#include <ui/icon.h>
#include <ui/label.h>
#include <js/js.h>

static auto make_label(const char* s, unsigned int fg = 0x000000, unsigned int bg = 0xFFFFFF) {
  auto lbl = new ui::label(s, ui::TextAlign::Center);
  lbl->set_foreground(fg);
  lbl->set_background(bg);

  return lbl;
};



struct vec3 {
  float x, y, z;
};

struct triangle {
  vec3 p[3];
};

struct mesh {
  ck::vec<triangle> tris;
};


struct mat4x4 {
  float m[4][4];

  mat4x4(void) {
    memset(this, 0, sizeof(*this));
  }
};

class game_view final : public ui::view {
 private:
  mesh meshCube;
  mat4x4 matProj;

  vec3 vCamera;

  float fTheta;


  ck::ref<ck::timer> draw_timer;


  void MultiplyMatrixVector(vec3& i, vec3& o, mat4x4& m) {
    o.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + m.m[3][0];
    o.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + m.m[3][1];
    o.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + m.m[3][2];
    float w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + m.m[3][3];

    if (w != 0.0f) {
      o.x /= w;
      o.y /= w;
      o.z /= w;
    }
  }


  void apply_perspective(vec3& v) {
  }

 public:
  ck::ref<gfx::bitmap> bmp = nullptr;


  virtual void mounted(void) {
    // bmp = gfx::load_png_from_res("cat.png");
    if (bmp) {
      // bmp = bmp->scale(bmp->width() / 3, bmp->height() / 3, gfx::bitmap::SampleMode::Nearest);
    }

    // Initialize the cube
    meshCube.tris.clear();
    // SOUTH
    meshCube.tris.push({0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f});
    meshCube.tris.push({0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f});
    // EAST
    meshCube.tris.push({1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f});
    meshCube.tris.push({1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f});
    // NORTH
    meshCube.tris.push({1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f});
    meshCube.tris.push({1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f});
    // WEST
    meshCube.tris.push({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f});
    meshCube.tris.push({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    // TOP
    meshCube.tris.push({0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    meshCube.tris.push({0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f});
    // BOTTOM
    meshCube.tris.push({1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    meshCube.tris.push({1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f});


    float fNear = 0.1f;
    float fFar = 1000.0f;
    float fFov = 90.0f;
    float fAspectRatio = (float)width() / (float)height();
    float fFovRad = 1.0f / tanf(fFov * 0.5f / 180.0f * M_PI);


    memset(&matProj, 0, sizeof(matProj));

    matProj.m[0][0] = fAspectRatio * fFovRad;
    matProj.m[1][1] = fFovRad;
    matProj.m[2][2] = fFar / (fFar - fNear);
    matProj.m[3][2] = (-fFar * fNear) / (fFar - fNear);
    matProj.m[2][3] = 1.0f;
    matProj.m[3][3] = 0.0f;

    set_background(0x000000);

    draw_timer = ck::timer::make_interval(1000 / 60, [this] { this->repaint(); });

    // repaint();
  }


  virtual ~game_view(void) {
  }


  virtual void paint_event(void) {
    auto s = get_scribe();

    printf("paint %d %d\n", width(), height());


    s.clear(rand());

    invalidate();
    return;

    // s.clear(0xFFFFFF);

    if (bmp) {
      s.blit(gfx::point(0, 0), *bmp, bmp->rect());
    }




    // Set up rotation matrices
    mat4x4 matRotZ, matRotX;
    fTheta += 1.0f * 0.016f;

    // Rotation Z
    matRotZ.m[0][0] = cosf(fTheta);
    matRotZ.m[0][1] = sinf(fTheta);
    matRotZ.m[1][0] = -sinf(fTheta);
    matRotZ.m[1][1] = cosf(fTheta);
    matRotZ.m[2][2] = 1;
    matRotZ.m[3][3] = 1;

    // Rotation X
    matRotX.m[0][0] = 1;
    matRotX.m[1][1] = cosf(fTheta * 0.5f);
    matRotX.m[1][2] = sinf(fTheta * 0.5f);
    matRotX.m[2][1] = -sinf(fTheta * 0.5f);
    matRotX.m[2][2] = cosf(fTheta * 0.5f);
    matRotX.m[3][3] = 1;


    for (auto& tri : meshCube.tris) {
      triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;

      // Rotate in Z-Axis
      MultiplyMatrixVector(tri.p[0], triRotatedZ.p[0], matRotZ);
      MultiplyMatrixVector(tri.p[1], triRotatedZ.p[1], matRotZ);
      MultiplyMatrixVector(tri.p[2], triRotatedZ.p[2], matRotZ);

      // Rotate in X-Axis
      MultiplyMatrixVector(triRotatedZ.p[0], triRotatedZX.p[0], matRotX);
      MultiplyMatrixVector(triRotatedZ.p[1], triRotatedZX.p[1], matRotX);
      MultiplyMatrixVector(triRotatedZ.p[2], triRotatedZX.p[2], matRotX);

      // Offset into the screen
      triTranslated = triRotatedZX;
      triTranslated.p[0].z = triRotatedZX.p[0].z + 3.0f;
      triTranslated.p[1].z = triRotatedZX.p[1].z + 3.0f;
      triTranslated.p[2].z = triRotatedZX.p[2].z + 3.0f;


      vec3 normal, line1, line2;
      line1.x = triTranslated.p[1].x - triTranslated.p[0].x;
      line1.y = triTranslated.p[1].y - triTranslated.p[0].y;
      line1.z = triTranslated.p[1].z - triTranslated.p[0].z;


      line2.x = triTranslated.p[2].x - triTranslated.p[0].x;
      line2.y = triTranslated.p[2].y - triTranslated.p[0].y;
      line2.z = triTranslated.p[2].z - triTranslated.p[0].z;



      // calculate the normal vector
      normal.x = line1.y * line2.z - line1.z * line2.y;
      normal.y = line1.z * line2.x - line1.x * line2.z;
      normal.z = line1.x * line2.y - line1.y * line2.x;

      // normalize the normal
      float l = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
      normal.x /= l;
      normal.y /= l;
      normal.z /= l;

      if (normal.x * (triTranslated.p[0].x - vCamera.x) +
              normal.y * (triTranslated.p[0].y - vCamera.y) +
              normal.z * (triTranslated.p[0].z - vCamera.z) <
          0) {
        // Project triangles from 3D --> 2D

        // project(triTranslated);
        MultiplyMatrixVector(triTranslated.p[0], triProjected.p[0], matProj);
        MultiplyMatrixVector(triTranslated.p[1], triProjected.p[1], matProj);
        MultiplyMatrixVector(triTranslated.p[2], triProjected.p[2], matProj);

        // Scale into view
        triProjected.p[0].x += 1.0f;
        triProjected.p[0].y += 1.0f;
        triProjected.p[1].x += 1.0f;
        triProjected.p[1].y += 1.0f;
        triProjected.p[2].x += 1.0f;
        triProjected.p[2].y += 1.0f;
        triProjected.p[0].x *= 0.5f * (float)width();
        triProjected.p[0].y *= 0.5f * (float)height();
        triProjected.p[1].x *= 0.5f * (float)width();
        triProjected.p[1].y *= 0.5f * (float)height();
        triProjected.p[2].x *= 0.5f * (float)width();
        triProjected.p[2].y *= 0.5f * (float)height();

        s.draw_line(triProjected.p[0].x, triProjected.p[0].y, triProjected.p[1].x,
                    triProjected.p[1].y, 0);
        s.draw_line(triProjected.p[0].x, triProjected.p[0].y, triProjected.p[2].x,
                    triProjected.p[2].y, 0);
        s.draw_line(triProjected.p[1].x, triProjected.p[1].y, triProjected.p[2].x,
                    triProjected.p[2].y, 0);
      }
    }


    invalidate();
  }
};



static js_ret_t native_print(js_context* ctx) {
  js_push_string(ctx, " ");
  js_insert(ctx, 0);
  js_join(ctx, js_get_top(ctx) - 1);
  printf("%s\n", js_to_string(ctx, -1));
  return 0;
}

static void eval_string(js_context* ctx, const char* expr) {
  // printf("=== eval: '%s' ===\n", expr);
  js_push_string(ctx, expr);
  int rc = js_peval(ctx);
  if (rc != 0) {
    js_safe_to_stacktrace(ctx, -1);
  } else {
    js_safe_to_string(ctx, -1);
  }
  const char* res = js_get_string(ctx, -1);
  printf("\e[32m%s\e[0m\n", res ? res : "null");
  js_pop(ctx);
}


ck::string read_line(int fd, const char *prompt) {
	printf("%s", prompt);
	fflush(stdout);
	char *buf = (char*)malloc(4096);
	memset(buf, 0, 4096);
	fgets(buf, 4096, stdin);
	buf[strlen(buf) - 1] = '\0';
	ck::string s = (const char *)buf;
	free(buf);
	return s;
}

int main(int argc, char** argv) {
  js_context* ctx = NULL;
  int i;
  js_int_t rc;

  ctx = js_create_heap_default();
  printf("ctx: %p\n", ctx);
  if (ctx == NULL) {
    printf("context is null!\n");
    return -1;
  }


  js_push_c_function(ctx, native_print, JS_VARARGS);
  js_put_global_string(ctx, "print");

	eval_string(ctx, "x = 'hello'");
	eval_string(ctx, "y = 'world'");
	eval_string(ctx, "print(x + ' ' + y)");

	while (1) {
		ck::string s = read_line(0, ">> ");
		eval_string(ctx, s.get());
	}


  js_destroy_heap(ctx);

	return 0;

  ui::application app;

  ui::window* win = app.new_window("Test", 120, 85);
  win->defer_invalidation(false);
  app.start();
  return 0;
}
