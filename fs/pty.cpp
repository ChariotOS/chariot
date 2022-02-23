#include <dev/driver.h>
#include <errno.h>
#include <fs/pty.h>
#include <ioctl.h>
#include <ck/map.h>
#include <module.h>


// #define PTY_DEBUG

#ifdef PTY_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG fmt, ##args);
#else
#define DBG(fmt, args...)
#endif


static ssize_t pts_read(fs::File &f, char *dst, size_t sz);
static ssize_t pts_write(fs::File &f, const char *dst, size_t sz);
static int pts_ioctl(fs::File &f, unsigned int cmd, off_t arg);
static ssize_t mx_read(fs::File &f, char *dst, size_t sz);
static ssize_t mx_write(fs::File &f, const char *dst, size_t sz);
static int pts_poll(fs::File &f, int events, poll_table &pt);


void pty::write_in(char c) {
  // DBG("pts::write_in '%c'\n", c);
  //
  in.write(&c, 1, true);
}

void pty::write_out(char c, bool block) {
  // DBG("pts::write_out '%c'\n", c);
  //
  out.write(&c, 1, block);
}

pty::~pty(void) {}



static spinlock pts_lock;
static ck::map<int, ck::ref<struct pty>> pts;


// static struct fs::FileOperations pts_ops = {
//     .read = pts_read,
//     .write = pts_write,
//     .ioctl = pts_ioctl,
//     .poll = pts_poll,
// };



static struct dev::DriverInfo pts_driver { .name = "pts", .type = DRIVER_CHAR, .major = MAJOR_PTS };


static auto getpts(int id) { return pts.get(id); }

static int allocate_pts() {
  pts_lock.lock();
  int i = 0;
  for (i = 0; true; i++) {
    // if there isn't a pts at this location, allocate one
    if (!pts.contains(i)) {
      pts[i] = new pty();
      pts[i]->index = 0;
      dev::register_name(pts_driver, ck::string::format("vtty%d", i), i);
      break;
    } else {
      // if nobody is controlling this pts, return it
      if (!pts[i]->controlled) {
        break;
      }
    }
  }

  // take control of the pts
  pts[i]->lock.lock();
  pts[i]->controlled = true;
  pts[i]->lock.unlock();

  pts_lock.unlock();

  return i;
}


#if 0
static int pts_ioctl(fs::File &f, unsigned int cmd, off_t arg) { return 0; }

static void close_pts(int ptsid) {
  pts_lock.lock();

  auto pts = getpts(ptsid);
  pts->lock.lock();
  pts->controlled = false;
  pts->lock.unlock();


  pts_lock.unlock();
}


static ssize_t pts_read(fs::File &f, char *dst, size_t sz) {
  auto pts = getpts(f.ino->minor);
  DBG("pts_read %dB from %p\n", sz, pts.get());
  return pts->in.read(dst, sz, true /* block? */);
}

static ssize_t pts_write(fs::File &f, const char *dst, size_t sz) {
  auto pts = getpts(f.ino->minor);
  DBG("pts_write %dB to %p\n", sz, pts.get());
  for (size_t s = 0; s < sz; s++)
    pts->output(dst[s]);
  return sz;
}


static ssize_t mx_read(fs::File &f, char *dst, size_t sz) {
  auto pts = getpts(f.pflags);
  DBG("mx_read %dB from %p\n", sz, pts.get());
  return pts->out.read(dst, sz, false /* block? */);
}

static ssize_t mx_write(fs::File &f, const char *dst, size_t sz) {
  auto pts = getpts(f.pflags);
  DBG("mx_write %dB to %p\n", sz, pts.get());
  for (size_t s = 0; s < sz; s++)
    pts->handle_input(dst[s]);
  return sz;
}



static int pts_poll(fs::File &f, int events, poll_table &pt) {
  auto pts = getpts(f.ino->minor);
  return pts->in.poll(pt) & events & AWAITFS_READ;
}


static int mx_poll(fs::File &f, int events, poll_table &pt) {
  auto pts = getpts(f.pflags);
  return pts->out.poll(pt) & events & AWAITFS_READ;
}

static int mx_open(fs::File &f) {
  DBG("mx open\n");
  f.pflags = allocate_pts();
  return 0;
}

static void mx_close(fs::File &f) {
  DBG("mx close\n");
  close_pts(f.pflags);
}

static int mx_ioctl(fs::File &f, unsigned int cmd, off_t arg) {
  DBG("mx ioctl\n");
  if (cmd == PTMX_GETPTSID) {
    return f.pflags;
  }
  return -EINVAL;
}




static struct fs::FileOperations mx_ops = {
    .read = mx_read,
    .write = mx_write,
    .ioctl = mx_ioctl,
    .open = mx_open,
    .close = mx_close,
    .poll = mx_poll,
};


static struct dev::DriverInfo mx_driver { .name = "ptmx", .type = DRIVER_CHAR, .major = MAJOR_PTMX, .char_ops = &mx_ops, };


static void pty_init(void) {
  dev::register_driver(mx_driver);
  dev::register_driver(pts_driver);
  //
  dev::register_name(mx_driver, "ptmx", 0);
}

module_init("pty", pty_init);
#endif
