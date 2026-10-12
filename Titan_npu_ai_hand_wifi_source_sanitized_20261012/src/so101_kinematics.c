#include "so101_kinematics.h"
#include <math.h>
#include <string.h>

#define SO101_PI             3.14159265358979323846
#define SO101_IK_EPS         1.0e-4
#define SO101_IK_LAMBDA      1.0e-3
#define SO101_IK_ITERS       20
#define SO101_IK_MAX_STEP    0.1
#define SO101_IK_SUCCESS_E2   1.0e-5

static const double g_so101_q_min[SO101_IK_JOINTS] = {-1.57, -1.57, -1.5, -3.14158};
static const double g_so101_q_max[SO101_IK_JOINTS] = { 1.57,  1.57,  1.5,  3.14158};
static const double g_so101_err_weight[6] = {1.0, 0.15, 1.0, 0.35, 0.45, 0.08};

static void mat_identity(double t[4][4])
{
    memset(t, 0, sizeof(double) * 16U);
    for (int i = 0; i < 4; i++)
    {
        t[i][i] = 1.0;
    }
}

static void mat_mul(double out[4][4], const double a[4][4], const double b[4][4])
{
    double r[4][4];

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            r[i][j] = 0.0;
            for (int k = 0; k < 4; k++)
            {
                r[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    memcpy(out, r, sizeof(r));
}

static void mat_tx(double t[4][4], double x)
{
    mat_identity(t);
    t[0][3] = x;
}

static void mat_tz(double t[4][4], double z)
{
    mat_identity(t);
    t[2][3] = z;
}

static void mat_rx(double t[4][4], double q)
{
    double c = cos(q);
    double s = sin(q);

    mat_identity(t);
    t[1][1] = c;
    t[1][2] = -s;
    t[2][1] = s;
    t[2][2] = c;
}

static void mat_ry(double t[4][4], double q)
{
    double c = cos(q);
    double s = sin(q);

    mat_identity(t);
    t[0][0] = c;
    t[0][2] = s;
    t[2][0] = -s;
    t[2][2] = c;
}

static void chain_mul(double t[4][4], const double next[4][4])
{
    double tmp[4][4];
    mat_mul(tmp, t, next);
    memcpy(t, tmp, sizeof(tmp));
}

static void so101_fk_matrix(const double q[SO101_IK_JOINTS], double t[4][4])
{
    double a[4][4];

    mat_identity(t);
    mat_tx(a, 0.02943); chain_mul(t, a);
    mat_tz(a, 0.05504); chain_mul(t, a);
    mat_ry(a, q[0]);    chain_mul(t, a);
    mat_tz(a, 0.1127);  chain_mul(t, a);
    mat_tx(a, 0.02798); chain_mul(t, a);
    mat_ry(a, q[1]);    chain_mul(t, a);
    mat_tx(a, 0.13504); chain_mul(t, a);
    mat_tz(a, 0.00519); chain_mul(t, a);
    mat_ry(a, q[2]);    chain_mul(t, a);
    mat_tx(a, 0.0593);  chain_mul(t, a);
    mat_tz(a, 0.00996); chain_mul(t, a);
    mat_rx(a, q[3]);    chain_mul(t, a);
}

static void pose_from_matrix(const double t[4][4], so101_pose_t *pose)
{
    double beta;
    double cb;

    pose->x = t[0][3];
    pose->y = t[1][3];
    pose->z = t[2][3];

    beta = atan2(-t[2][0], sqrt(t[0][0] * t[0][0] + t[1][0] * t[1][0]));
    cb = cos(beta);
    if (fabs(cb) > 1.0e-8)
    {
        pose->yaw = atan2(t[1][0] / cb, t[0][0] / cb);
        pose->roll = atan2(t[2][1] / cb, t[2][2] / cb);
    }
    else
    {
        pose->yaw = 0.0;
        pose->roll = atan2(t[0][1], t[1][1]);
    }
    pose->pitch = beta;
}

void so101_kin_fk(const double q[SO101_IK_JOINTS], so101_pose_t *pose)
{
    double t[4][4];

    if (pose == 0)
    {
        return;
    }

    so101_fk_matrix(q, t);
    pose_from_matrix(t, pose);
}

double so101_kin_clamp(double value, double min_value, double max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

double so101_kin_wrap_pi(double value)
{
    while (value > SO101_PI)
    {
        value -= 2.0 * SO101_PI;
    }
    while (value < -SO101_PI)
    {
        value += 2.0 * SO101_PI;
    }
    return value;
}

static void pose_error(const so101_pose_t *target, const so101_pose_t *now, double e[6])
{
    e[0] = target->x - now->x;
    e[1] = target->y - now->y;
    e[2] = target->z - now->z;
    e[3] = so101_kin_wrap_pi(target->roll - now->roll);
    e[4] = so101_kin_wrap_pi(target->pitch - now->pitch);
    e[5] = so101_kin_wrap_pi(target->yaw - now->yaw);

    for (int i = 0; i < 6; i++)
    {
        e[i] *= g_so101_err_weight[i];
    }
}

static int solve_4x4(double a[4][4], double b[4], double x[4])
{
    double m[4][5];

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            m[i][j] = a[i][j];
        }
        m[i][4] = b[i];
    }

    for (int col = 0; col < 4; col++)
    {
        int pivot = col;
        double pivot_abs = fabs(m[col][col]);
        for (int row = col + 1; row < 4; row++)
        {
            double v = fabs(m[row][col]);
            if (v > pivot_abs)
            {
                pivot_abs = v;
                pivot = row;
            }
        }

        if (pivot_abs < 1.0e-12)
        {
            return 0;
        }

        if (pivot != col)
        {
            for (int k = col; k < 5; k++)
            {
                double tmp = m[col][k];
                m[col][k] = m[pivot][k];
                m[pivot][k] = tmp;
            }
        }

        double div = m[col][col];
        for (int k = col; k < 5; k++)
        {
            m[col][k] /= div;
        }

        for (int row = 0; row < 4; row++)
        {
            if (row == col)
            {
                continue;
            }
            double factor = m[row][col];
            for (int k = col; k < 5; k++)
            {
                m[row][k] -= factor * m[col][k];
            }
        }
    }

    for (int i = 0; i < 4; i++)
    {
        x[i] = m[i][4];
    }
    return 1;
}

int so101_kin_ik(const double q_now[SO101_IK_JOINTS], const so101_pose_t *target, double q_out[SO101_IK_JOINTS])
{
    double q[SO101_IK_JOINTS];
    so101_pose_t now;
    double e[6];

    if ((q_now == 0) || (target == 0) || (q_out == 0))
    {
        return 0;
    }

    for (int i = 0; i < SO101_IK_JOINTS; i++)
    {
        q[i] = so101_kin_clamp(q_now[i], g_so101_q_min[i], g_so101_q_max[i]);
    }

    for (int iter = 0; iter < SO101_IK_ITERS; iter++)
    {
        double j[6][SO101_IK_JOINTS];
        double a[4][4];
        double b[4];
        double dq[4];
        double err_norm = 0.0;

        so101_kin_fk(q, &now);
        pose_error(target, &now, e);
        for (int r = 0; r < 6; r++)
        {
            err_norm += e[r] * e[r];
        }
        if (err_norm < 1.0e-6)
        {
            break;
        }

        for (int c = 0; c < SO101_IK_JOINTS; c++)
        {
            double qp[SO101_IK_JOINTS];
            so101_pose_t pp;
            double ep[6];

            memcpy(qp, q, sizeof(qp));
            qp[c] += SO101_IK_EPS;
            so101_kin_fk(qp, &pp);
            pose_error(target, &pp, ep);

            for (int r = 0; r < 6; r++)
            {
                j[r][c] = (ep[r] - e[r]) / SO101_IK_EPS;
            }
        }

        for (int r = 0; r < 4; r++)
        {
            b[r] = 0.0;
            for (int c = 0; c < 4; c++)
            {
                a[r][c] = (r == c) ? SO101_IK_LAMBDA : 0.0;
            }
        }

        for (int r = 0; r < 4; r++)
        {
            for (int k = 0; k < 6; k++)
            {
                b[r] -= j[k][r] * e[k];
                for (int c = 0; c < 4; c++)
                {
                    a[r][c] += j[k][r] * j[k][c];
                }
            }
        }

        if (!solve_4x4(a, b, dq))
        {
            return 0;
        }

        for (int i = 0; i < SO101_IK_JOINTS; i++)
        {
            dq[i] = so101_kin_clamp(dq[i], -SO101_IK_MAX_STEP, SO101_IK_MAX_STEP);
            q[i] = so101_kin_clamp(q[i] + dq[i], g_so101_q_min[i], g_so101_q_max[i]);
        }
    }

    so101_kin_fk(q, &now);
    pose_error(target, &now, e);
    {
        double final_err = 0.0;
        for (int r = 0; r < 6; r++)
        {
            final_err += e[r] * e[r];
        }
        if (final_err > SO101_IK_SUCCESS_E2)
        {
            return 0;
        }
    }

    for (int i = 0; i < SO101_IK_JOINTS; i++)
    {
        q_out[i] = q[i];
    }
    return 1;
}