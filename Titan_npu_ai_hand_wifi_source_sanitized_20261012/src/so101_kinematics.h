#ifndef SRC_SO101_KINEMATICS_H_
#define SRC_SO101_KINEMATICS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SO101_IK_JOINTS 4

typedef struct
{
    double x;
    double y;
    double z;
    double roll;
    double pitch;
    double yaw;
} so101_pose_t;

void so101_kin_fk(const double q[SO101_IK_JOINTS], so101_pose_t *pose);
int so101_kin_ik(const double q_now[SO101_IK_JOINTS], const so101_pose_t *target, double q_out[SO101_IK_JOINTS]);
double so101_kin_clamp(double value, double min_value, double max_value);
double so101_kin_wrap_pi(double value);

#ifdef __cplusplus
}
#endif

#endif /* SRC_SO101_KINEMATICS_H_ */