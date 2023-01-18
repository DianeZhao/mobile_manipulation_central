#!/usr/bin/env python3
import rospkg
import numpy as np
import mobile_manipulation_central as mm
import time


TIMESTEP = 0.001
TOOL_JOINT_NAME = "tool_gripper_joint"
DURATION = 10


def main():
    # find the URDF (this has to be compiled first using the script
    # mobile_manipulation_central/urdf/compile_xacro.sh)
    rospack = rospkg.RosPack()
    mm_path = rospack.get_path("mobile_manipulation_central")
    urdf_path = mm_path + "/urdf/compiled/thing_pyb.urdf"

    # load initial joint configuration
    home = mm.load_home_position()

    # create the simulation
    sim = mm.simulation.BulletSimulation(TIMESTEP)
    robot = mm.simulation.BulletSimulatedRobot(urdf_path, TOOL_JOINT_NAME)
    robot.reset_joint_configuration(home)

    r = robot.link_pose()[0]  # initial position
    rd = r + [1, 0, 0]  # desired position
    k = 1  # gain

    t = 0
    while t <= DURATION:
        # basic differential inverse kinematics control
        r = robot.link_pose()[0]
        v = k * (rd - r)
        J = robot.jacobian()[:3, :]  # only take position Jacobian
        u = J.T @ np.linalg.solve(J @ J.T, v)
        robot.command_velocity(u)

        # step the sim forward in time
        t = sim.step(t)
        time.sleep(TIMESTEP)


main()
