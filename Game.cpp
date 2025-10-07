#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 1;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	//send_button(left);
	//send_button(right);
	//send_button(up);
	//send_button(down);
	send_button(jump);
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 1) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 1!");
	
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	/*recv_button(recv_buffer[4+0], &left);
	recv_button(recv_buffer[4+1], &right);
	recv_button(recv_buffer[4+2], &up);
	recv_button(recv_buffer[4+3], &down);*/
	recv_button(recv_buffer[4+0], &jump);

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}


//-----------------------------------------

GameState Game::build_state() const {
	GameState s;
	
	s.for_left_side = true;
	s.for_right_side = true;

	for (auto const& p : players) {
		const bool is_left = (p.side == Player::Side::Left);
		const auto& flip = p.flip;

		if (is_left) {
			s.pancake1_pos = flip.pancake_pos;
			s.pancake1_rot = flip.pancake_rot;
			s.pan1_rot = flip.pan_rot;
		}
		else {
			s.pancake2_pos = flip.pancake_pos;
			s.pancake2_rot = flip.pancake_rot;
			s.pan2_rot = flip.pan_rot;
		}
	}
	return s;
}


static void start_flip(FlipSim& F, glm::vec3 start_pos, glm::quat start_rot) {
	if (F.active) return;
	F.active = true; F.t = 0.f;

	// T = 2 * v / g symmetric parabolic trjectory vy = vy0 - gt
	F.T = 2.f * F.vz0 / F.g;

	F.start_pos = start_pos;
	F.start_rot = start_rot;
}

static void step_flip(float dt, FlipSim& F) {
	if (!F.active) return;
	F.t += dt;
	float t = glm::min(F.t, F.T);

	float z = F.start_pos.z + F.vz0 * t - 0.5f * F.g * t * t;
	float theta = (t / F.T) * F.spin;

	F.pancake_pos = F.start_pos; F.pancake_pos.z = z;
	F.pancake_rot = glm::normalize(glm::angleAxis(theta, glm::vec3(1, 0, 0)) * F.start_rot);

	float pan_angle = glm::radians(14.f) * std::sin(glm::pi<float>() * (t / F.T));
	F.pan_rot = glm::angleAxis(pan_angle, glm::vec3(1, 0, 0));

	if (F.t >= F.T) {
		F.active = false;
		F.pancake_pos = F.start_pos;
		F.pancake_rot = glm::normalize(glm::angleAxis(F.spin, glm::vec3(1, 0, 0)) * F.start_rot);
		F.pan_rot = glm::quat(1, 0, 0, 0);
	}
}


Game::Game() : mt(0x15466666) {
}

// show player on the UI whether they have arrived 
Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	const int idx = int(players.size()) - 1;
	std::cout << "player size is: " << players.size() << std::endl;
	if (idx == 0) {
		player.name = "Player 1";
		player.color = glm::normalize(glm::vec3(0.2f, 0.6f, 1.0f)); // blue
		player.side = Player::Side::Left;
		std::cout << "player 1 set " << std::endl;
	}
	else {
		player.name = "Player 2";
		player.color = glm::normalize(glm::vec3(1.0f, 0.5f, 0.8f)); // pink
		player.side = Player::Side::Right;
		std::cout << "player 2 set " << std::endl;
	}

	player.score = 0;
	player.space_downs = 0;

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {

	(void)elapsed;
	for (auto& p : players) {
		if (p.controls.jump.downs > 0 && !p.flip.active) {
			glm::vec3 base_pos = (p.side == Player::Side::Left) ? pancake1_base_pos: pancake2_base_pos;
			glm::quat base_rot = (p.side == Player::Side::Left) ? pancake1_base_rot : pancake2_base_rot;
			start_flip(p.flip, base_pos, base_rot);
		}
		step_flip(elapsed, p.flip);
		p.controls.jump.downs = 0;
	}

}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	//send player info helper:
	auto send_player = [&](Player const &player) {
		connection.send(player.score);
		connection.send(player.side);
		connection.send(player.color);
	
		//NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		//effectively: truncates player name to 255 chars
		uint8_t len = uint8_t(std::min< size_t >(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	auto send_f32 = [&](float f) {
		uint32_t u;
		std::memcpy(&u, &f, 4);
		connection.send(uint8_t(u));
		connection.send(uint8_t(u >> 8));
		connection.send(uint8_t(u >> 16));
		connection.send(uint8_t(u >> 24));
		};

	auto send_vec3 = [&](const glm::vec3 & v) {
		send_f32(v.x); send_f32(v.y); send_f32(v.z);
		};

	auto send_quat_xyzw = [&](const glm::quat& q) {
		send_f32(q.x); send_f32(q.y); send_f32(q.z); send_f32(q.w);
		};

	GameState s = build_state();

	connection.send(uint8_t(s.for_left_side ? 1 : 0));
	connection.send(uint8_t(s.for_right_side ? 1 : 0));

	// send pan1 transforms
	send_vec3(s.pancake1_pos);
	send_quat_xyzw(s.pancake1_rot);
	send_quat_xyzw(s.pan1_rot);

	// send pan2 transformrs
	send_vec3(s.pancake2_pos);
	send_quat_xyzw(s.pancake2_rot);
	send_quat_xyzw(s.pan2_rot);

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//uint32_t at = 0;
	auto read_bytes = [&](void* dst, size_t n) {
		if (at + n > size) throw std::runtime_error("Ran out of bytes reading state message.");
		std::memcpy(dst, &recv_buffer[4 + at], n);
		at += uint32_t(n);
		};

	auto read_u8 = [&]() {
		uint8_t b; read_bytes(&b, 1); return b;
		};
	auto read_f32 = [&]() {
		float f; read_bytes(&f, 4); return f;
		};

	auto read_vec3 = [&]() {
		glm::vec3 v; v.x = read_f32(); v.y = read_f32(); v.z = read_f32(); return v;
		};
	auto read_quat_xyzw = [&]() {
		glm::quat q;
		// x,y,z,w -> store into (w,x,y,z)
		float x = read_f32(), y = read_f32(), z = read_f32(), w = read_f32();
		q.w = w; q.x = x; q.y = y; q.z = z;
		return glm::normalize(q);
		};

	////copy bytes from buffer and advance position:
	//auto read = [&](auto *val) {
	//	if (at + sizeof(*val) > size) {
	//		throw std::runtime_error("Ran out of bytes reading state message.");
	//	}
	//	std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
	//	at += sizeof(*val);
	//};

	players.clear();
	uint8_t player_count = read_u8();
	//read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read_bytes(&player.score, sizeof(player.score));
		read_bytes(&player.side, sizeof(player.side));
		read_bytes(&player.color, sizeof(player.color));
		uint8_t name_len = read_u8();
		//read(&name_len);
		//n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		//player.name = "";
		player.name.clear();
		for (uint8_t n = 0; n < name_len; ++n) {
			char c;
			read_bytes(&c,1);
			//player.name += c;
			player.name.push_back(c);
		}
	}

	// transforms
	last_state.for_left_side = (read_u8() != 0);
	last_state.for_right_side = (read_u8() != 0);

	last_state.pancake1_pos = read_vec3();
	last_state.pancake1_rot = read_quat_xyzw();
	last_state.pan1_rot = read_quat_xyzw();

	last_state.pancake2_pos = read_vec3();
	last_state.pancake2_rot = read_quat_xyzw();
	last_state.pan2_rot = read_quat_xyzw();

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
