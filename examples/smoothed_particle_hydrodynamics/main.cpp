#include <cmath>
#include <iostream>
#include <silo.h>
#include <stdlib.h>
#include <vector>

#include "kernels.h"

int box_indicator(float x, float y)
{
    return (x < 0.25) && (y < 1.0);
}

void place_particles(
    int count,
    float *pos_x,
    float *pos_y,
    float *v_x,
    float *v_y,
    float hh)
{
    int index = 0;

    for (float x = 0; x < 1; x += hh) {
        for (float y = 0; y < 1; y += hh) {
            if (box_indicator(x,y)) {
                pos_x[index] = x;
                pos_y[index] = y;
                v_x[index] = 0;
                v_y[index] = 0;
                ++index;
            }
        }
    }
}

int count_particles(float hh)
{
    int ret = 0;

    for (float x = 0; x < 1; x += hh) {
        for (float y = 0; y < 1; y += hh) {
            ret += box_indicator(x,y);
        }
    }

    return ret;
}

void normalize_mass(float *mass, int n, float *rho, float rho0)
{
    float rho_squared_sum = 0;
    float rho_sum = 0;

    for (int i = 0; i < n; ++i) {
        rho_squared_sum += rho[i] * rho[i];
        rho_sum += rho[i];
    }

    *mass = *mass * rho0 * rho_sum / rho_squared_sum;
}

void dump_time_step(int cycle, int n, float* pos_x, float* pos_y)
{
    DBfile *dbfile = NULL;
    char filename[100];
    sprintf(filename, "output%04d.silo", cycle);
    dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                      "simulation time step", DB_HDF5);

    float *coords[] = {(float*)pos_x, (float*)pos_y};
    DBPutPointmesh(dbfile, "pointmesh", 2, coords, n,
                   DB_FLOAT, NULL);

    DBClose(dbfile);
}

int main(int argc, char** argv)
{

    // time step length:
    float dt = 1e-4;
    // pitch: (size of particles)
    float h = 2e-2;
    // target density:
    float rho0 = 1000;
    // bulk modulus:
    float k = 1e3;
    // viscosity:
    float mu = 0.1;
    // gravitational acceleration:
    float g = 9.8;

    float hh = h / 1.3;
    int count = count_particles(hh);

    std::vector<float> rho(count);
    std::vector<float> pos_x(count);
    std::vector<float> pos_y(count);
    std::vector<float> v_x(count);
    std::vector<float> v_y(count);
    std::vector<float> a_x(count);
    std::vector<float> a_y(count);

    place_particles(count, pos_x.data(), pos_y.data(), v_x.data(), v_y.data(), hh);
    float mass = 1;
    compute_density(count, rho.data(), pos_x.data(), pos_y.data(), h, mass);
    normalize_mass(&mass, count, rho.data(), rho0);

    int num_steps = 20000;
    int io_period = 15;

    for (int t = 0; t < num_steps; ++t) {
        if ((t % io_period) == 0) {
            dump_time_step(t, count, pos_x.data(), pos_y.data());
        }

        compute_density(
            count,
            rho.data(),
            pos_x.data(),
            pos_y.data(),
            h,
            mass);

        compute_accel(
            count,
            rho.data(),
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data(),
            a_x.data(),
            a_y.data(),
            mass,
            g,
            h,
            k,
            rho0,
            mu);

        leapfrog(
            count,
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data(),
            a_x.data(),
            a_y.data(),
            dt);

        reflect_bc(
            count,
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data());
    }

    dump_time_step(num_steps, count, pos_x.data(), pos_y.data());

    return 0;
}