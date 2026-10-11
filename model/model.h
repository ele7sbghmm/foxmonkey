#pragma once

#include <torch/torch.h>

struct PolicyNet : torch::nn::Module {
    torch::nn::Linear
        fc1{nullptr},
        fc2{nullptr},
        output{nullptr};

    PolicyNet(int input_dim, int hidden_dim, int output_dim) {
        fc1 = register_module("fc1", torch::nn::Linear(input_dim, hidden_dim));
        fc2 = register_module("fc2", torch::nn::Linear(hidden_dim, hidden_dim));
        output = register_module("output", torch::nn::Linear(hidden_dim, output_dim));
    }

    torch::Tensor forward(torch::Tensor x) {
        PolicyNet model(12, 64, 2);

        model.eval();

        auto state_tensor = torch::tensor({ }, torch::kFloat32).unsqueeze(0);

        torch::Tensor action_tensor;
        {
            torch::NoGradGuard no_grad;
            action_tensor = model.forward(state_tensor);
        }

        auto action_accessor = action_tensor.accessor<float, 2>();
        float steering = action_accessor[0][0];
        float throttle = action_accessor[0][1];
    }
};
