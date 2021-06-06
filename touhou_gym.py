import socket
import numpy as np
import struct
import subprocess
import random
import os

import gym
from gym import spaces

from stable_baselines.common.policies import CnnLstmPolicy
from stable_baselines.common.vec_env import SubprocVecEnv
from stable_baselines import PPO2
from stable_baselines.common.callbacks import CheckpointCallback
from stable_baselines.common.env_checker import check_env
from stable_baselines.common.schedules import LinearSchedule
#from stable_baselines.deepq.policies import LnCnnPolicy
#from stable_baselines import DQN

from touhou_env import TouhouEnv 

# TODO: save to different file per call, print on save
'''
class SimpleSaveCallback(EventCallback):
    def __init__(self, freq: int, filename: str, verbose: int = 1):
        super(SimpleSaveCallback, self).__init__(verbose=verbose)
        self.freq = freq
        self.filename = filename
    def _on_step(self) -> bool:
        # NOTE: n_calls is the steps divided by n_envs (in this case 12)
        if self.n_calls % self.freq == 0:
            self.model.save(self.filename)
        return True
'''

'''
steps = 0
def callback(a, b):
    global steps
    steps += 1
    print(steps)
    if steps % 100 == 0:
        print("saving")
        a.save("ppo2_touhou6_10_autosave")
'''

# here for testing, taken from openai-gym source
class RandomAgent(object):
    """The world's simplest agent!"""
    def __init__(self, action_space):
        self.action_space = action_space

    def act(self, observation, reward, done):
        return self.action_space.sample()

def linear_schedule(initial_value):
    def func(progress):
        return progress * initial_value
    return func

if __name__ == '__main__':
    env = SubprocVecEnv([TouhouEnv for i in range(16)])
    model = PPO2(CnnLstmPolicy, env, tensorboard_log="./ppo2_lstm_touhou6_21/", nminibatches=16, learning_rate=linear_schedule(2.5e-4), cliprange=linear_schedule(0.1))
    #model = PPO2.load("models/ppo2_lstm_touhou6_model_16_45360000_steps")
    #model.tensorboard_log = "./ppo2_lstm_touhou6_17/"
    #model.env = env
    #model.learning_rate = LinearSchedule(1000000000, 0.05).value
    callback = CheckpointCallback(save_freq=10000, save_path="./models", name_prefix='ppo2_lstm_touhou6_model_21')
    model.learn(total_timesteps=1000000000, callback=callback, reset_num_timesteps=False)
    model.save("ppo2_lstm_touhou6_model_20_final")

'''
if __name__ == '__main__':
    env = TouhouEnv()
    agent = RandomAgent(env.action_space)
    reward = 0
    done = False
    for i in range(1000):
        ob = env.reset()
        while True:
            action = agent.act(ob, reward, done)
            ob, reward, done, _ = env.step(action)
            env.render()
            if done:
                break
    env.close()
'''

'''
if __name__ == '__main__':
    model_name = "dqn_touhou6"
    env = TouhouEnv()
    model = DQN(LnCnnPolicy, env, prioritized_replay=True, tensorboard_log="./" + model_name + "/")
    #model = DQN(LnCnnPolicy, env, prioritized_replay=True)
    #model = DQN.load(model_name + "_autosave")
    #model.tensorboard_log = "./" + model_name + "/"
    #model.env = env
    # actually 120k tensorboard steps
    #callback = SimpleSaveCallback(10000, model_name + "_autosave")
    model.learn(total_timesteps=999999999, callback=callback, reset_num_timesteps=False)
    model.save(model_name + "_final")
    '''