#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <list>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

struct GameState {
	bool for_left_side = false;
	bool for_right_side = false;

	glm::vec3 pancake1_pos = glm::vec3(0.0f);
	glm::quat pancake1_rot = glm::quat(1, 0, 0, 0);
	glm::quat pan1_rot = glm::quat(1, 0, 0, 0);

	glm::vec3 pancake2_pos = glm::vec3(0.0f);
	glm::quat pancake2_rot = glm::quat(1, 0, 0, 0);
	glm::quat pan2_rot = glm::quat(1, 0, 0, 0);
};

struct FlipSim {
	bool  active = false;
	float t = 0.0f;
	float T = 0.0f;
	float g = 6.0f;
	float vz0 = 4.0f;
	float spin = glm::radians(180.f);

	glm::vec3 pancake_pos = {};
	glm::quat pancake_rot = glm::quat(1, 0, 0, 0);

	glm::quat pan_rot = glm::quat(1, 0, 0, 0);

	glm::vec3 start_pos = {};
	glm::quat start_rot = glm::quat(1.0, 0, 0, 0);
};


//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		//Button left, right, up, down, jump;
		Button jump;
		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	//player state (sent from server):
	/*glm::vec3 position;
	glm::vec3 velocity;*/

	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	std::string name = "";

	enum class Side : uint8_t { Left = 0, Right = 1 };
	Side side = Side::Left;

	uint8_t score = 0;
	uint32_t space_downs = 0;

	FlipSim flip;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	GameState build_state() const;
	GameState last_state;

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;
	glm::vec3 pancake1_base_pos = glm::vec3(-1.205, -0.022, 0.750);
	glm::vec3 pancake2_base_pos = glm::vec3(1.205, -0.022, 0.750);
	glm::quat pancake1_base_rot = glm::quat(0.003f, 1.0, 0, 0);
	glm::quat pancake2_base_rot = glm::quat(0.003f, 1.0, 0, 0);

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
