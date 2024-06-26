#ifndef LAYERS_HPP
#define LAYERS_HPP

#include "conv.hpp"
#include "model.hpp"
#include "tensor.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <unsupported/Eigen/CXX11/Tensor>

namespace demucscpp
{

void apply_dconv(const struct demucscpp::demucs_model &model,
                 Eigen::Tensor3dXf &y, int freq_idx, int encdec_idx,
                 int layer_idx, int mid_crop);

// used for implementing both self-attention and cross-attention
// let's not modify the second argument
void common_encoder_layer(
    Eigen::Tensor3dXf &q,       // q = x = frequency|time
    const Eigen::Tensor3dXf &k, // k = xt = time|frequency, _or_ k == q
    const Eigen::Tensor1dXf &norm1_weight, const Eigen::Tensor1dXf &norm1_bias,
    const Eigen::Tensor1dXf &norm2_weight, const Eigen::Tensor1dXf &norm2_bias,
    const Eigen::MatrixXf &in_proj_weight, const Eigen::VectorXf &in_proj_bias,
    const Eigen::MatrixXf &out_proj_weight,
    const Eigen::VectorXf &out_proj_bias, const Eigen::VectorXf &gamma_1_scale,
    const Eigen::Tensor1dXf &norm3_weight, const Eigen::Tensor1dXf &norm3_bias,
    const Eigen::MatrixXf &linear1_weight, const Eigen::VectorXf &linear1_bias,
    const Eigen::MatrixXf &linear2_weight, const Eigen::VectorXf &linear2_bias,
    const Eigen::VectorXf &gamma_2_scale,
    const Eigen::Tensor1dXf &norm_out_weight,
    const Eigen::Tensor1dXf &norm_out_bias, const int num_heads,
    float eps = 1e-5, const bool self_attention = false);

Eigen::Tensor3dXf group_norm(const Eigen::Tensor3dXf &x,
                             const Eigen::Tensor1dXf &w,
                             const Eigen::Tensor1dXf &b, int num_groups,
                             float eps);

Eigen::Tensor3dXf group_norm_fused_gelu(const Eigen::Tensor3dXf &x,
                                        const Eigen::Tensor1dXf &w,
                                        const Eigen::Tensor1dXf &b, float eps);

Eigen::Tensor3dXf layer_norm(const Eigen::Tensor3dXf &x,
                             const Eigen::Tensor1dXf &weight,
                             const Eigen::Tensor1dXf &b, float eps);

Eigen::Tensor3dXf glu(const Eigen::Tensor3dXf &x, const int dim);

inline Eigen::Tensor3dXf gelu(const Eigen::Tensor3dXf &x)
{
    return x.unaryExpr(
        [](float a)
        { return 0.5f * a * (1.0f + std::erf(a / std::sqrt(2.0f))); });
}

inline Eigen::MatrixXf gelu(const Eigen::MatrixXf &x)
{
    return x.unaryExpr(
        [](float a)
        { return 0.5f * a * (1.0f + std::erf(a / std::sqrt(2.0f))); });
}

inline Eigen::Tensor3dXf layer_scale(const Eigen::Tensor3dXf &x,
                                     const Eigen::Tensor1dXf &scale_weights)
{
    Eigen::Tensor3dXf y_out(x.dimensions());
    for (int i = 0; i < x.dimension(1); ++i)
    {
        y_out.chip<1>(i) = x.chip<1>(i) * scale_weights(i);
    }
    return y_out;
}

inline float calculate_variance(const Eigen::Tensor3dXf &tensor, float mean)
{
    Eigen::Tensor<float, 0> sum_squares = (tensor - mean).square().sum();
    float variance = sum_squares(0) / (tensor.size() - 1);
    return variance;
}

inline float calculate_variance(const Eigen::Tensor2dXf &tensor, float mean)
{
    Eigen::Tensor<float, 0> sum_squares = (tensor - mean).square().sum();
    float variance = sum_squares(0) / (tensor.size() - 1);
    return variance;
}

inline float calculate_variance(const Eigen::Tensor1dXf &tensor, float mean)
{
    Eigen::Tensor<float, 0> sum_squares = (tensor - mean).square().sum();
    float variance = sum_squares(0) / (tensor.size() - 1);
    return variance;
}
} // namespace demucscpp

namespace demucscpp_v3
{
void apply_dconv_v3(const struct demucscpp_v3::demucs_v3_model &model,
                    Eigen::Tensor3dXf &y, int freq_idx, int layer_idx,
                    int mid_crop);

void apply_dconv_v3_encoder_4_5(
    const struct demucscpp_v3::demucs_v3_model &model, Eigen::Tensor3dXf &y,
    int encoder_idx, int mid_crop,
    struct demucscpp_v3::demucs_v3_segment_buffers &buffers);

// new function for LocalState, a local attention layer used
// in demucs v3
void local_attention(Eigen::Tensor3dXf &x, // x = frequency, time, or combined
                     const Eigen::Tensor3dXf &content_weight,
                     const Eigen::Tensor1dXf &content_bias,
                     const Eigen::Tensor3dXf &query_weight,
                     const Eigen::Tensor1dXf &query_bias,
                     const Eigen::Tensor3dXf &key_weight,
                     const Eigen::Tensor1dXf &key_bias,
                     const Eigen::Tensor3dXf &query_decay_weight,
                     const Eigen::Tensor1dXf &query_decay_bias,
                     const Eigen::Tensor2dXf &query_decay_kernel,
                     const Eigen::Tensor3dXf &proj_weight,
                     const Eigen::Tensor1dXf &proj_bias, const int hidden_size);

// this shit is complicated, give it its own namespace
namespace groupnorm
{
using ActivationFunc = std::function<float(float)>;

// this  applies the group norm on the second dimension
template <typename ActivationFunc>
inline Eigen::Tensor3dXf generalized_group_norm(const Eigen::Tensor3dXf &x,
                                                const Eigen::Tensor1dXf &weight,
                                                const Eigen::Tensor1dXf &bias,
                                                int num_groups, float eps,
                                                ActivationFunc activation_func)
{
    int freq = x.dimension(0);
    int channels = x.dimension(1);
    int width = x.dimension(2);

    Eigen::Tensor3dXf y_out(freq, channels, width);
    y_out.setZero();

    int group_size = channels / num_groups;

    for (int g = 0; g < num_groups; ++g)
    {
        int start = g * group_size;
        int end = (g + 1) * group_size;

        Eigen::Tensor3dXf slice =
            x.slice(Eigen::array<int, 3>{0, start, 0},
                    Eigen::array<int, 3>{freq, group_size, width});

        Eigen::Tensor<float, 0> mean_tensor = slice.mean();
        float mean = mean_tensor(0);
        float var = demucscpp::calculate_variance(slice, mean);

        for (int i = 0; i < freq; ++i)
        {
            for (int c = start; c < end; ++c)
            {
                for (int w = 0; w < width; ++w)
                {
                    float norm_val = (x(i, c, w) - mean) / std::sqrt(var + eps);
                    norm_val = norm_val * weight(c) + bias(c);
                    y_out(i, c, w) = activation_func(norm_val);
                }
            }
        }
    }

    return y_out;
}

inline float gelu(float a)
{
    return 0.5f * a * (1.0f + std::erf(a / std::sqrt(2.0f)));
}

inline Eigen::Tensor3dXf group_norm(const Eigen::Tensor3dXf &x,
                                    const Eigen::Tensor1dXf &weight,
                                    const Eigen::Tensor1dXf &bias,
                                    int num_groups, float eps)
{
    return generalized_group_norm(x, weight, bias, num_groups, eps,
                                  [](float x) { return x; });
}

inline Eigen::Tensor3dXf group_norm_fused_gelu(const Eigen::Tensor3dXf &x,
                                               const Eigen::Tensor1dXf &weight,
                                               const Eigen::Tensor1dXf &bias,
                                               int num_groups, float eps)
{
    return generalized_group_norm(x, weight, bias, num_groups, eps,
                                  [](float x)
                                  { return demucscpp_v3::groupnorm::gelu(x); });
}

inline Eigen::Tensor3dXf group_norm_2(const Eigen::Tensor3dXf &x,
                                      const Eigen::Tensor1dXf &weight,
                                      const Eigen::Tensor1dXf &bias,
                                      int num_groups, float eps)
{
    Eigen::array<int, 3> shuffle_dims = {1, 0, 2}; // Shuffle dimensions

    // Shuffle, apply group norm, and unshuffle
    Eigen::Tensor3dXf x_shuffled = x.shuffle(shuffle_dims);
    Eigen::Tensor3dXf y_shuffled = generalized_group_norm(
        x_shuffled, weight, bias, num_groups, eps, [](float x) { return x; });
    return y_shuffled.shuffle(shuffle_dims);
}

inline Eigen::Tensor3dXf group_norm_fused_gelu_2(
    const Eigen::Tensor3dXf &x, const Eigen::Tensor1dXf &weight,
    const Eigen::Tensor1dXf &bias, int num_groups, float eps)
{
    Eigen::array<int, 3> shuffle_dims = {1, 0, 2}; // Shuffle dimensions

    // Shuffle, apply group norm with GELU, and unshuffle
    Eigen::Tensor3dXf x_shuffled = x.shuffle(shuffle_dims);
    Eigen::Tensor3dXf y_shuffled = generalized_group_norm(
        x_shuffled, weight, bias, num_groups, eps,
        [](float x) { return demucscpp_v3::groupnorm::gelu(x); });
    return y_shuffled.shuffle(shuffle_dims);
}
} // namespace groupnorm
} // namespace demucscpp_v3

#endif // LAYERS_HPP
