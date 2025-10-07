#include "Mode.hpp"
#include "Scene.hpp"
#include "Connection.hpp"
#include "Game.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>


struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//---- mesh references ----
	Scene scene;
	Scene::Camera* camera = nullptr;
	Scene::Camera* camera_left = nullptr;
	Scene::Camera* camera_right = nullptr;
	bool camera_chosen = false;

	Scene::Transform* pan_1 = nullptr;
	Scene::Transform* pan_2 = nullptr;
	Scene::Transform* pancake_1 = nullptr;
	Scene::Transform* pancake_2 = nullptr;

	//---- animation ----
	//FlipAnim flip1, flip2;

	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

};
