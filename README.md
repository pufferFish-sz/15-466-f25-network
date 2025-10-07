# Pancake Flip

Author: Shangyi Zhu

Design: 

This is a 2-player competitive game in a fixed camera 3D scene where the person that flips the most number of perfect pancakes wins. 

Networking: 

The server limits up to 2 connections, and will immediately disconnect any additional joins. This is done in server.cpp where it checks the number of current joins.
There is additionally a small UI in the left-hand corner that displays pink/blue if the player is connected, and gray if disconnected. 
When the players join, they will be assigned an side information (left/right) in Game::spawn_player(). There are 2 camera set up for each player initially in blender, 
and they will be assigned to each player respectively in PlayMode.cpp after getting information from the server 
about which side they are on.

The position and rotation of the pancakes and pans are also computed in Game.cpp and gets drawn on PlayMode.cpp. When a player presses the space 
button, it will detect it in PlayMode::handle_event() and then sends to the server with controls.send_controls_message(&client.connection). After the server finishes computing, 
it will send the byte information of the position and rotation back to the client via Game::send_state_message. 
and PlayMode.cpp later reads the information from Game::recv_state_message.

Due to time constraints, the functions for scoring perfect pancakes has yet to be implemented, but logic would be around the same, which is to compute it in the server, and then
reflect it in the UI of each client. 

Screen Shot:

![Screen Shot](PancakeFlip.png)

How To Play:

Each person will have to flip the pancakes in a pan in front of them within 1 second of hearing "ding!" to get the perfect browned side of their pancake. 
Each round has 5 pancakes, and whoever gets the most number of perfect pancakes wins. Press space to flip the pancake.


This game was built with [NEST](NEST.md).

