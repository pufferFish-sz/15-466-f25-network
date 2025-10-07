#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include <iostream>
#include <iomanip>

//static void print_transform_info(
//	const std::string& name,
//	const glm::vec3& pos,
//	const glm::quat& rot
//) {
//	std::cout << std::fixed << std::setprecision(3);
//	std::cout << name << " base position: ("
//		<< pos.x << ", " << pos.y << ", " << pos.z << ")\n";
//
//	std::cout << name << " base rotation (quat): [w="
//		<< rot.w << ", x=" << rot.x
//		<< ", y=" << rot.y << ", z=" << rot.z << "]\n";
//
//	// Optional: also print Euler angles in degrees for readability
//	glm::vec3 euler = glm::degrees(glm::eulerAngles(rot));
//	std::cout << name << " base rotation (Euler XYZ deg): ("
//		<< euler.x << ", " << euler.y << ", " << euler.z << ")\n";
//}

// The scene setup code template is taken from game 3
GLuint pancake_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > pancake_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("pancake.pnct"));
	pancake_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);

	return ret;
	});

Load< Scene > pancake_scene(LoadTagDefault, []() -> Scene const* {
	return new Scene(data_path("pancake.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
		Mesh const& mesh = pancake_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable& drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = pancake_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		});
	});

PlayMode::PlayMode(Client &client_) : client(client_) {
	scene = *pancake_scene;
	for (auto& cam : scene.cameras) {
		if (!cam.transform) continue;
		const std::string& n = cam.transform->name;
		if (n == "CameraLeft") camera_left = &cam;
		if (n == "CameraRight") camera_right = &cam;
	}
	if (!camera_left || !camera_right) {
		throw std::runtime_error("Need two cameras in the scene.");
	}

	//get pointers to leg for convenience:
	for (auto& transform : scene.transforms) {
		//std::cout << transform.name << std::endl;
		if (transform.name == "Pan1") pan_1 = &transform;
		else if (transform.name == "Pan2") pan_2 = &transform;
		else if (transform.name == "Pancake1") pancake_1 = &transform;
		else if (transform.name == "Pancake2") pancake_2 = &transform;
	}
	if (pan_1 == nullptr) throw std::runtime_error("pan 1 not found.");
	if (pan_2 == nullptr) throw std::runtime_error("pan 2 foot not found.");
	if (pancake_1 == nullptr) throw std::runtime_error("pancake 1 not found.");
	if (pancake_2 == nullptr) throw std::runtime_error("pancake 2 not found.");
	
	/*print_transform_info("Pancake1", pancake_1->position, pancake_1->rotation);
	print_transform_info("Pancake2", pancake_1->position, pancake_2->rotation);*/

}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} /*else if (evt.key.key == SDLK_A) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		}*/ else if (evt.key.key == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
	/*	if (evt.key.key == SDLK_A) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.pressed = false;
			return true;
		} else*/ if (evt.key.key == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.jump.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
					
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	// choose camera depending on the player
	if (!camera_chosen && !game.players.empty()) {
		const Player& curr_player = game.players.front();
		
		if (curr_player.side == Player::Side::Left) {
			camera = camera_left;
		}
		else {
			camera = camera_right;
		}

		camera_chosen = (camera != nullptr);
	}

	if (pan_1 && pan_2 && pancake_1 && pancake_2) {
		const GameState& s = game.last_state;

		if (s.for_left_side) {
			pan_1->rotation = s.pan1_rot;
			pancake_1->position = s.pancake1_pos;
			pancake_1->rotation = s.pancake1_rot;
		}
		if (s.for_right_side) {
			pan_2->rotation = s.pan2_rot;
			pancake_2->position = s.pancake2_pos;
			pancake_2->rotation = s.pancake2_rot;

			static const glm::quat R180Y = glm::angleAxis(glm::pi<float>(), glm::vec3(0, 0, 1));
			pan_2->rotation = glm::normalize(R180Y * pan_2->rotation);
			pancake_2->rotation = glm::normalize(R180Y * pancake_2->rotation);
		}
	}

}

void PlayMode::draw(glm::uvec2 const& drawable_size) {
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	)); 

	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	auto draw_dot = [&](glm::vec2 center, float radius, glm::u8vec4 color) {
		for (uint32_t a = 0; a < circle.size(); ++a) {
			glm::vec2 p0 = center + radius * circle[a];
			glm::vec2 p1 = center + radius * circle[(a + 1) % circle.size()];
			lines.draw(glm::vec3(p0, 0.0f), glm::vec3(p1, 0.0f), color);
		}
		};


	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	// compute a camera-forward directional light in WORLD SPACE:
	glm::mat4 cam_to_world = camera->transform->make_parent_from_local();
	glm::vec3 cam_forward = glm::normalize(glm::vec3(cam_to_world * glm::vec4(0, 0, -1, 0))); // -Z in camera space

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		//float aspect = float(drawable_size.x) / float(drawable_size.y);
		/*DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));*/

		// --- title ---
		constexpr float H = 0.09f; // title text height
		glm::vec3 title_origin(-aspect + 0.1f * H, -1.0f + 0.1f * H, 0.0f);
		glm::vec3 x_dir(H, 0.0f, 0.0f);
		glm::vec3 y_dir(0.0f, H, 0.0f);

		const char* title = "Pancake Flip";
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(title, title_origin, x_dir, y_dir, glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text(title, title_origin + glm::vec3(ofs, ofs, 0.0f), x_dir, y_dir,
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		// place UI stack above the title
		glm::vec2 ui_anchor = glm::vec2(title_origin) + glm::vec2(0.0f, 1.6f * H);

		// layout constants
		const float char_advance = 0.6f * H;
		const float label_dot_gap = 0.4f * H;
		const float row_gap = 1.2f * H; 
		const float r = 0.35f * H;   // dot radius

		// connectivity flags
		bool left_connected = false, right_connected = false;
		for (auto const& p : game.players) {
			if (p.side == Player::Side::Left)  left_connected = true;
			if (p.side == Player::Side::Right) right_connected = true;
		}

		glm::u8vec4 blue = { 64, 170, 255, 255 };
		glm::u8vec4 pink = { 255, 128, 200, 255 };
		glm::u8vec4 gray = { 120, 120, 120, 255 };

		// tiny label helper (shadowed text)
		auto draw_label = [&](glm::vec2 at, std::string const& text) {
			constexpr float LH = 0.08f;
			lines.draw_text(text, glm::vec3(at.x, at.y, 0),
				glm::vec3(LH, 0, 0), glm::vec3(0, LH, 0), glm::u8vec4(0, 0, 0, 0));
			float lofs = 2.0f / drawable_size.y;
			lines.draw_text(text, glm::vec3(at.x + lofs, at.y + lofs, 0),
				glm::vec3(LH, 0, 0), glm::vec3(0, LH, 0), glm::u8vec4(255, 255, 255, 0));
			};

		// row 1 (P1)
		std::string t1 = "P1";
		glm::vec2 L1 = ui_anchor;                                
		float     W1 = char_advance * float(t1.size());            
		glm::vec2 C1 = glm::vec2(L1.x + W1 + label_dot_gap, L1.y + r); 

		draw_label(L1, t1);
		draw_dot(C1, r, left_connected ? blue : gray);

		// row 2 (P2)
		std::string t2 = "P2";
		glm::vec2 L2 = ui_anchor + glm::vec2(0.0f, row_gap);
		float     W2 = char_advance * float(t2.size());
		glm::vec2 C2 = glm::vec2(L2.x + W2 + label_dot_gap, L2.y+ r);

		draw_label(L2, t2);
		draw_dot(C2, r, right_connected ? pink : gray);
	}
	GL_ERRORS();
}