#define _USE_MATH_DEFINES
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Common.h"

struct _Shape;
typedef struct _Shape Shape;

typedef struct {
    void (*ToString)(Shape*);
    double (*CalculateArea)(Shape*);
} VTable;

typedef struct _Shape {
    VTable* vtable;
    char name[20];
} Shape;

void shapePrint(Shape* c) {
    printf("%s\n", c->name);
}

void initShape(Shape* s, const char* name) {
    s->vtable = (VTable*)malloc(sizeof(VTable));
    strncpy_s(s->name, sizeof(s->name) - 1, name, sizeof(s->name) - 1);
    s->name[sizeof(s->name) - 1] = 0;
    s->vtable->ToString = shapePrint;
    s->vtable->CalculateArea = NULL;
}

void releaseShape(Shape* s, const char* name) {
    free(s->vtable);
}

typedef struct _Square {
    Shape shape;
    int width;
    int height;
} Square;

double squareCalculate(Square* s) {
    return s->height * s->width;
}

void initSquare(Square* s, const char* name, int w, int h) {
    initShape((Shape*)s, name);
    s->height = h;
    s->width = w;
    ((Shape*)s)->vtable->CalculateArea = (double (*)(Shape*))squareCalculate;
}

void releaseSquare(Square* s) {
    free(((Shape*)s)->vtable);
}

typedef struct _Circle {
    Shape shape;
    int radius;
} Circle;

double circleCalculate(Circle* c) {
    return M_PI_2 * c->radius * c->radius;
}

void initCircle(Circle* c, const char* name, int r) {
    initShape((Shape*)c, name);
    c->radius = r;
    ((Shape*)c)->vtable->CalculateArea = (double (*)(Shape*))circleCalculate;
}

void releaseCircle(Circle* s) {
    free(((Shape*)s)->vtable);
}

namespace CPolymorphism_T {

void TCase0() {
    Square square;
    initSquare(&square, "square", 4, 5);
    Shape* shape = (Shape*)&square;
    shape->vtable->ToString(shape);
    printf("%f\n", shape->vtable->CalculateArea(shape));
    releaseSquare(&square);

    Circle circle;
    initCircle(&circle, "circle", 4);
    shape = (Shape*)&circle;
    shape->vtable->ToString(shape);
    printf("%f\n", shape->vtable->CalculateArea(shape));
    releaseCircle(&circle);
}

}  // namespace CPolymorphism_T

void CPolymorphism_Test() {
    UT_Case_ALL(CPolymorphism_T);
}