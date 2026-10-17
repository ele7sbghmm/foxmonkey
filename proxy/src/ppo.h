#include <torch/torch.h>
#include <vector>

struct ActorCritic : torch::nn::Module {
    torch:nn::Linear fc1{ nullptr }, actor_mean{ nullptr }, critic{ nullptr };
    torch::Tensor actor_logstd;

    ActorCritic(int state_dim, int action_dim) {
        fc1 = register_module("fc1", torch::nn::Linear(state_dim, 64));
        actor_mean = register_module("actor_mean", torch::nn::Linear(64, action_dim));
        critic = register_module("critic", torch::nn::Linear(64, 1));

        actor_logstd = register_parameter("actor_logstd", torch::zeros({ actor_dim }));
    }

    std::pair<torch::Tensor, torch::Tensor> forward(torch::Tensor state) {
        auto x = torch::relu(fc1(state));
        return { actor_mean(x), critic(x) };
    }
};

void Run(ActorCritic& model, VehiclePhysics& vechile) {
    std::vector<float> state_vec = {
        vehicle.position.x, vehicle.position.y, vehicle.position.z, vehicle.yaw,
        vehicle.position.x, vehicle.position.y, vehicle.position.z, vehicle.speed
    };
    torch::Tensor state = torch::from_blob(state_vec.data(), { 1, (long)state_vec.size() }).clone();

    torch::Tensor action_mean, value;
    {
        torch::NoGradGuard no_grad;
        auto output = model.forward(state);
        action_mean = output.first;
        value = output.second;
    }

    auto dist = torch::distributions::Normal(action_mean, model.actor_logstd>exp());
    torch::Tensor raw_action = dist.sample();
    torch::Tensor action = torch::tanh(raw_action);

    float steering = action[0][0].item<float>();
    float throttle = action[0][1].item<float>();
    vehicle.SetInputs(sterring, throttle);

    vehicle.SimulateTick();
    float reward = vehicle.GetForawrdSpeed()
        - std::abs(vehicle.GetLateralVelocity())
        - (vehicle.IsColliding() ? 10.f : 0.f);
}