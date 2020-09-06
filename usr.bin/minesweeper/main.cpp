
#include <ui/application.h>


// non-number tiles
enum class tile : char {
  unknown,
  empty,
  flag,
  marked_unknown,
  marked_empty,
  mine,
  mine_exploded,
  mine_bad_flag,
};


#define MS_CLEAR '?'
#define MS_MINE '!'
#define MS_FLAG 'F'
#define MS_UNKNOWN '#'

class minesweeper_game : public ui::view {
  int rows = 8;
  int columns = 8;
  int mines = 10;
  char* grid;
  // char *visible;
 public:
  minesweeper_game() {
    grid = (char*)malloc(rows * columns);
    generate_grid();
  }

  ~minesweeper_game(void) { free(grid); }



  void generate_grid(void) {
    int ncells = rows * columns;
    // Initialize the grid.
    for (int i = 0; i < ncells; i++) grid[i] = MS_CLEAR;
    // Add the mines.
    for (int i = 0; i < mines; i++) grid[i] = MS_MINE;

    // Shuffle the mines. (Fisher-Yates)
    for (int i = ncells - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      int tmp = grid[j];
      grid[j] = grid[i];
      grid[i] = tmp;
    }

    print_grid();
  }


  void print_grid(void) {
    printf("\n");
    for (int row = 0; row < rows; row++) {
      for (int col = 0; col < columns; col++) {
        printf("%c", grid[columns * row + col]);
      }
      printf("\n");
    }
    printf("\n");
  }


  virtual void paint_event(void) override {
    auto scribe = get_scribe();

    scribe.clear(rand());

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override { repaint(); }
};




int main() {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("Minesweeper uwu", 152, 195);

  auto& game = win->set_view<minesweeper_game>();
  (void)game;

  // start the application!
  app.start();

  return 0;
}
