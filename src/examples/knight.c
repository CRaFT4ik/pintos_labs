#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <string.h>

/* Implemented by Eldar Timraleev © 2017. */

/* Программа должна строить маршрут обхода конем шахматного поля
   (конь должен пройти через все поля доски по одному разу) – находить
   любое решение.
   Аргументы: размер поля, начальное положение коня.

   Реализация: алгоритм перебора с возвратом. Можно оптимизировать:
   выбирать направление с наименьшим возможным числом ходов. */

typedef struct _COORD
{
    short x;
    short y;
} COORD, *PCOORD;

/* Все возможные движения коня. */
enum knight_moves
{
    UP_LEFT,
    UP_RIGHT,

    DOWN_LEFT,
    DOWN_RIGHT,

    LEFT_UP,
    LEFT_DOWN,

    RIGHT_UP,
    RIGHT_DOWN,

    STAY
};

static void manager(void);
static void backTracking(PCOORD, int);
static int * matrix(PCOORD);
static void moveCoord(PCOORD, enum knight_moves);
static void printMatrix(void);

#define MATRIX_DIM 500                      // Максимально допустимый размер поля.
static int matrix_[MATRIX_DIM][MATRIX_DIM]; // Матрица-траектория.

static int SIZE;                            // Размер шахматного поля.
static int WEIGHT;                          // Вес шахматного поля в байтах.
static COORD coord_s;                       // Начальная позиция коня.

int main(int argc UNUSED, const char* argv[])
{
    SIZE = atoi(argv[1]);
    WEIGHT = SIZE * SIZE * sizeof(int);

    coord_s.x = atoi(argv[2]) - 1;
    coord_s.y = atoi(argv[3]) - 1;

    if ((coord_s.x < 0 || coord_s.x > SIZE - 1) || (coord_s.y < 0 || coord_s.y > SIZE - 1))
    {
        printf("Ошибка. Проверьте начальные координаты!\n");
        exit(-1);
    } else if (MATRIX_DIM < SIZE)
    {
        printf("Ошибка. Максимальная размерность поля [%d] превышена!\n", MATRIX_DIM);
        exit(-1);
    }

    printf("\n * * * * * * * * * * * * * * * * * *  \n");
    printf("     Обход конём шахматного поля        \n");
    printf("         by Eldar Timraleev             \n");
    printf(" * * * * * * * * * * * * * * * * * *    \n\n");

    printf("Размеры поля: %d x %d\n", SIZE, SIZE);
    printf("Начальное положение коня: x = %d, y = %d\n", coord_s.x + 1, coord_s.y + 1);
    printf("Направление поля: y↓, x→\n\n");

    memset(matrix_, 0, sizeof(matrix_));
    manager();

    printf("\n * * * * * * * * * * * * * * * * * *  \n");

    return EXIT_SUCCESS;
}

static void manager(void)
{
    printf("Выполняется поиск решений...\n\n");
    backTracking(&coord_s, 1);
    printf("Решений не найдено :(\n");
}

/* Перебор с возвратом. */
static void backTracking(const PCOORD coord, int step_id)
{
    if (coord == NULL) return;

    *(int *) matrix(coord) = step_id;
    if (step_id == SIZE * SIZE) { printMatrix(); exit(0); }         // Проверяем, завершен ли обход.

    COORD move_coord;

    /* Идем во всех возможных направлениях с данных координат. */
    for (enum knight_moves direction = UP_LEFT; direction != STAY; direction++)
    {
        memcpy(&move_coord, coord, sizeof(COORD));
        moveCoord(&move_coord, direction);

        if (move_coord.x < 0 || move_coord.x > SIZE - 1 ||      // Если по Х мы шагнули за рамки поля, то объявлем ход некорректным.
            move_coord.y < 0 || move_coord.y > SIZE - 1 ||      // Если по Y мы шагнули за рамки поля, то объявлем ход некорректным.
            *(int *) matrix(&move_coord) != 0)                  // Если в данную ячейку мы уже наступали, то объявлем ход некорректным.
            continue;

        /* Здесь мы уверены, что в данную ячейку поля ходить можно. */

        backTracking(&move_coord, step_id + 1);                 // Продолжаем делать ходы в выбранном направлении.
    }

    *(int *) matrix(coord) = 0;
}

/* Возвращает адрес элемента в матрице по координатам COORD. */
static int * matrix(PCOORD coord)
{
    return &matrix_[coord->x][coord->y];
}

/* Изменить координаты COORD в соответствии с движением в направлении DIRECTION. */
static void moveCoord(PCOORD coord, enum knight_moves direction)
{
         if (direction == UP_LEFT   || direction == UP_RIGHT)   coord->y -= 2;
    else if (direction == LEFT_UP   || direction == RIGHT_UP)   coord->y -= 1;
    else if (direction == LEFT_DOWN || direction == RIGHT_DOWN) coord->y += 1;
    else if (direction == DOWN_LEFT || direction == DOWN_RIGHT) coord->y += 2;

         if (direction == LEFT_UP   || direction == LEFT_DOWN)  coord->x -= 2;
    else if (direction == UP_LEFT   || direction == DOWN_LEFT)  coord->x -= 1;
    else if (direction == UP_RIGHT  || direction == DOWN_RIGHT) coord->x += 1;
    else if (direction == RIGHT_UP  || direction == RIGHT_DOWN) coord->x += 2;
}

/* Отрисовывает шахматное поле. */
static void printMatrix(void)
{
    int x_markup = 1, y_markup = 1;

    for (int i = 1; i <= SIZE * SIZE; i++)
    {
        if (x_markup <= SIZE)
        {
            printf(" %.3d  ", x_markup);
            if (x_markup % SIZE == 0) printf("\n");
            x_markup++; i--;
        } else if (x_markup > SIZE && x_markup <= 2 * SIZE)
        {
            printf("  |   ");
            if (x_markup % SIZE == 0) printf("\n");
            x_markup++; i--;
        } else
        {
            printf("[%.3d] ", matrix_[(i - 1) % SIZE][(i - 1) / SIZE]);
            if (i % SIZE == 0) printf("— %.3d\n", y_markup++);
        }
    }
    printf("\n");
}
