import socket
import numpy as np
import struct
import subprocess
import random
import os
import cv2
import time

import gym
from gym import spaces

# wow global variables
stage = 0
width = 84
height = 84

# because sockets suck
def recvfull(sockObj, size):
    read = b''
    left = size
    while (len(read) != size):
        last_size = len(read)
        read += sockObj.recv(left)
        left -= len(read) - last_size
    return read

def arr_to_obv(arr):
    # this is awful
    # discarding items for now
    #x = np.dstack((arr[:width*height], arr[width*height:width*height*2], arr[width*height*2:width*height*3])).reshape((height,width,3))
    #arr.reshape((height, width, 4))
    #return np.rot90(np.flipud(np.transpose(arr.reshape((4, height, width)))), 3)
    return np.mean(arr.reshape((width, height, 4)), axis=2).reshape((width, height, 1))

class TouhouEnv(gym.Env):
    metadata = {'render.modes': ['human']}

    def __init__(self, params):
        # TODO: this can be made async so all games can load at once
        # is it worth the effort?
        global stage
        self.stage = stage % 6
        stage += 1
        print("using stage " + str(self.stage + 1))
        #port = random.randint(4000, 6000)
        port = os.getpid() + 4000
        # windows
        #subprocess.Popen(["D:\\Games\\touhou\\EoSD-AI\\東方紅魔郷.exe", str(port), str(self.stage)], cwd="D:\\Games\\touhou\\EoSD-AI")
        # ubuntu (native)
        #subprocess.Popen(["wine", "東方紅魔郷.exe", str(port), str(self.stage)], cwd="/media/khangaroo/extra/Games/touhou/EoSD-AI")
        # ubuntu (wsl2)
        subprocess.Popen(["/mnt/d/Games/touhou/EoSD-AI/thanksascii.exe", str(port), str(self.stage)], cwd="/mnt/d/Games/touhou/EoSD-AI")

        # arrow keys, focus
        # no shoot because that should always be held down
        # no skip because that should also always be held down
        # no bomb because the agent has no way of knowing bomb count (not trying to train it to read text, 
        # and adding special pixels probably won't work)
        self.stepped_once = False
        # needs to be discrete for dqn
        self.action_space = spaces.Discrete(10)
        #self.action_space = spaces.MultiDiscrete([5, 2])
        self.observation_space = spaces.Box(low=0, high=255, dtype=np.uint8, shape=(width,height,1))
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.bind(('127.0.0.1', port))
        print("waiting for client...")
        self.socket.listen(1)
        (self.conn, _) = self.socket.accept()
        print("got client, waiting for initial observation")

        # discard first observation from the client
        # the order we want this to go in is
        # agent logic runs -> send input -> game logic runs -> observation sent to server -> repeat
        recvfull(self.conn, height * width * 4)
        recvfull(self.conn, 5) # discard score and done data
        print("init done")
        return

    def step(self, action):
        self.stepped_once = True
        # needs to be discrete for dqn
        # note to self: if going back to normal, remember pack_action
        pack_action = [0] * 2
        pack_action[0] = action % 5
        if action >= 5:
            pack_action[1] = 1
        self.conn.sendall(struct.pack("BB", pack_action[0], pack_action[1]))
        #self.conn.sendall(struct.pack("BB", action[0], action[1]))
        obv = np.frombuffer(recvfull(self.conn, height * width * 4), np.uint8) # this should be done in c++ but i'm too dumb to implement it myself
        self.last_obv = arr_to_obv(obv)
        return self.last_obv, struct.unpack("<i", recvfull(self.conn, 4))[0], struct.unpack("B", recvfull(self.conn, 1))[0] == 1, {}

    def reset(self):
        # game will reset itself
        # it will send another observation when it's done
        if self.stepped_once:
            self.conn.sendall(struct.pack("BB", 0, 0)) # send dummy input
            arr = np.frombuffer(recvfull(self.conn, width * height * 4), np.uint8)
            recvfull(self.conn, 5) # discard score and done data
            return arr_to_obv(arr)
        else:
            return np.empty((height,width,1), dtype=np.uint8)

    def render(self, mode='human', close=False):
        #print(np.swapaxes(self.last_obv, 0, 2).shape)
        img = cv2.resize(np.mean(self.last_obv, axis=2), (width * 4, height * 4), cv2.INTER_NEAREST)
        cv2.normalize(img, img, 0, 255, cv2.NORM_MINMAX)
        cv2.imshow("output", img)
        cv2.waitKey(1)
        return