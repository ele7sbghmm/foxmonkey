import gymnasium as gym
import numpy as np

if __name__ == '__main__':
    env = gym.make('FrozenLake-v1', is_slippery=True)
    qt = np.zeros((env.observation_space.n, env.action_space.n))
    print('hello world')
