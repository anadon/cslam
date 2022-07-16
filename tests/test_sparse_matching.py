import unittest

from cslam.loop_closure_sparse_matching import LoopClosureSparseMatching
import random
import numpy as np
from collections import namedtuple

GlobalDescriptor = namedtuple('GlobalDescriptor',
                              ['image_id', 'robot_id', 'descriptor'])


def set_params():
    params = {}
    params['robot_id'] = 0
    params['nb_robots'] = 2
    params['frontend.similarity_threshold'] = 0.0
    params['frontend.similarity_loc'] = 1.0
    params['frontend.similarity_scale'] = 0.25
    return params


class TestSparseMatching(unittest.TestCase):
    """Unit tests for sparse matching
    """

    def test_distance_to_similarity(self):
        """Distance to similarity
        """
        params = set_params()
        lcsm = LoopClosureSparseMatching(params)
        for i in range(0, 100, 1):
            d = i / 10
            s = lcsm.distance_to_similarity(d)
            self.assertGreaterEqual(s, 0.0)
            self.assertLessEqual(s, 1.0)

    def test_add_local_global_descriptor(self):
        """Add local keyframe
        """
        params = set_params()
        lcsm = LoopClosureSparseMatching(params)
        descriptor = np.random.rand(10)
        lcsm.add_local_global_descriptor(descriptor, 1)
        for i in range(len(descriptor)):
            self.assertAlmostEqual(descriptor[i], lcsm.local_nnsm.data[0, i])

    def test_add_other_robot_global_descriptor(self):
        """Add other robot keyframe
        """
        params = set_params()
        lcsm = LoopClosureSparseMatching(params)
        descriptor = np.random.rand(10)
        msg = GlobalDescriptor(0, 1, descriptor.tolist())
        lcsm.add_other_robot_global_descriptor(msg)
        for i in range(len(descriptor)):
            self.assertAlmostEqual(descriptor[i],
                                   lcsm.other_robots_nnsm[1].data[0, i])

    def test_matches(self):
        """Matches between descriptors
        """
        params = set_params()
        lcsm = LoopClosureSparseMatching(params)

        descriptor0 = np.random.rand(10)
        lcsm.add_local_global_descriptor(descriptor0, 2)

        descriptor1 = 1 - descriptor0
        msg = GlobalDescriptor(3, 1, descriptor1.tolist())
        lcsm.add_other_robot_global_descriptor(msg)

        descriptor2 = descriptor0
        descriptor2[0] = 0.0
        descriptor2[1] = 0.0
        msg2 = GlobalDescriptor(4, 1, descriptor2.tolist())
        lcsm.add_other_robot_global_descriptor(msg2)

        id = lcsm.candidate_selector.candidate_edges[(0, 2, 1, 4)].robot1_id
        best_match_descriptor = lcsm.other_robots_nnsm[id].data[0]
        for i in range(len(descriptor1)):
            self.assertAlmostEqual(descriptor1[i], best_match_descriptor[i])

    def test_select_candidates0(self):
        """Select candidates
        """
        params = set_params()
        params['nb_robots'] = 3
        lcsm = LoopClosureSparseMatching(params)

        nb_local_kfs = 100
        for i in range(nb_local_kfs):
            descriptor = np.random.rand(10)
            lcsm.add_local_global_descriptor(descriptor, i)
        nb_other_kfs = 100
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 1, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 2, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)

        nb_candidates = 20
        is_robot_considered = {}
        for i in range(params['nb_robots']):
            is_robot_considered[i] = True
        selection = lcsm.select_candidates(nb_candidates, is_robot_considered)
        self.assertEqual(len(selection), nb_candidates)

    def test_select_candidates1(self):
        """Select candidates
            No robot 1 in range
        """
        params = set_params()
        params['nb_robots'] = 4
        lcsm = LoopClosureSparseMatching(params)

        nb_local_kfs = 100
        for i in range(nb_local_kfs):
            descriptor = np.random.rand(10)
            lcsm.add_local_global_descriptor(descriptor, i)
        nb_other_kfs = 100
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 2, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 3, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)

        nb_candidates = 20

        is_robot_considered = {}
        for i in range(params['nb_robots']):
            is_robot_considered[i] = True
        selection = lcsm.select_candidates(nb_candidates, is_robot_considered)
        self.assertEqual(len(selection), nb_candidates)

    def test_select_candidates2(self):
        """Select candidates
            No robot 0 in range
        """
        params = set_params()
        params['nb_robots'] = 4
        params['robot_id'] = 1
        lcsm = LoopClosureSparseMatching(params)

        nb_local_kfs = 100
        for i in range(nb_local_kfs):
            descriptor = np.random.rand(10)
            lcsm.add_local_global_descriptor(descriptor, i)
        nb_other_kfs = 100
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 2, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)
        for i in range(nb_other_kfs):
            descriptor = np.random.rand(10)
            msg = GlobalDescriptor(i, 3, descriptor.tolist())
            lcsm.add_other_robot_global_descriptor(msg)

        nb_candidates = 20

        is_robot_considered = {}
        for i in range(params['nb_robots']):
            is_robot_considered[i] = True

        selection = lcsm.select_candidates(nb_candidates, is_robot_considered)
        self.assertEqual(len(selection), nb_candidates)


if __name__ == "__main__":
    unittest.main()
