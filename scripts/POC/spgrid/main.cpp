#include <type_traits>
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include <tuple>
#include <vector>
#include <array>
#include <cassert>
#include <omp.h>
#include "common_utils.h"
#include "iterator_utils.h"
#include <GL/gl.h>
#include <GL/glut.h>
#include "SPGrid.h"

using namespace bate::spgrid;

#define ITV 200
#define N 256
float pixels[N * N];

SPFloat16Grid<N> f_old;
SPFloat16Grid<N> f_new;
SPFloatGrid<N> rho;
SPFloat4Grid<N> vel;

const float niu = 0.005f;
const float tau = 3.f * niu + 0.5f;
const float inv_tau = 1.f / tau;

std::array<int, 3> directions[] = {{0,0,0},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,1,1},{-1,-1,-1},{1,1,-1},{-1,-1,1},{1,-1,1},{-1,1,-1},{-1,1,1},{1,-1,-1}};
float weights[] = {2.f/9.f, 1.f/9.f, 1.f/9.f, 1.f/9.f, 1.f/9.f, 1.f/9.f, 1.f/9.f,1.f/72.f, 1.f/72.f, 1.f/72.f, 1.f/72.f, 1.f/72.f, 1.f/72.f, 1.f/72.f, 1.f/72.f};

void initFunc() {
    #pragma omp parallel for
    for (int z = 0; z < N; z++) {
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < N; x++) {
                rho.set(x, y, z, 1.f);
                vel.set(x, y, z, {0.f, 0.f, 0.f, 0.f});
            }
        }
    }
}

float f_eq(int x, int y, int z, int q) {
    float m = rho.at(x, y, z);
    auto [vx, vy, vz, vw] = vel.get(x, y, z);
    float eu = vx * directions[q][0]
        + vy * directions[q][1] + vz * directions[q][2];
    float uv = vx * vx + vy * vy + vz * vz;
    float term = 1.f + 3.f * eu + 4.5f * eu * eu - 1.5f * uv;
    float feq = weights[q] * m * term;
    return feq;
}

void stepFunc() {
    #pragma omp parallel for
    for (int z = 0; z < N; z++) {
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < N; x++) {
                for (int q = 0; q < 15; q++) {
                    int mdx = (x - directions[q][0] + N) % N;
                    int mdy = (y - directions[q][1] + N) % N;
                    int mdz = (z - directions[q][2] + N) % N;
                    f_new.at(q, x, y, z) = f_old.at(q, mdx, mdy, mdz)
                        * (1.f - inv_tau) + f_eq(mdx, mdy, mdz, q) * inv_tau;
                }
            }
        }
    }
    #pragma omp parallel for
    for (int z = 0; z < N; z++) {
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < N; x++) {
                float m = 0.f;
                float vx = 0.f, vy = 0.f, vz = 0.f;
                for (int q = 0; q < 15; q++) {
                    float f = f_new.at(q, x, y, z);
                    vx += f * directions[q][0];
                    vy += f * directions[q][1];
                    vz += f * directions[q][2];
                    m += f;
                }
                float mscale = 1 / std::max(m, 1e-6f);
                vx /= mscale; vy /= mscale; vz /= mscale;
                rho.set(x, y, z, m);
                vel.set(x, y, z, {vx, vy, vz, 0.f});
            }
        }
    }
}

void renderFunc() {
    int z = N / 2;
    #pragma omp parallel for
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            auto [vx, vy, vz, vw] = vel.get(x, y, z);
            float val = std::sqrt(vx * vx + vy * vy + vz * vz);
            pixels[y * N + x] = val;
        }
    }
}

void displayFunc() {
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawPixels(N, N, GL_RED, GL_FLOAT, pixels);
    glFlush();
}

void timerFunc(int unused) {
    stepFunc();
    renderFunc();
    glutPostRedisplay();
    glutTimerFunc(ITV, timerFunc, 0);
}

void keyboardFunc(unsigned char key, int x, int y) {
    if (key == 27)
        exit(0);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(N, N);
    glutCreateWindow("GLUT Window");
    glutDisplayFunc(displayFunc);
    glutKeyboardFunc(keyboardFunc);
    initFunc();
    renderFunc();
    glutTimerFunc(ITV, timerFunc, 0);
    glutMainLoop();
}
