/*
 * Copyright (c) 2017 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/core/NEON/kernels/NECol2ImKernel.h"

#include "arm_compute/core/Error.h"
#include "arm_compute/core/Helpers.h"
#include "arm_compute/core/ITensor.h"
#include "arm_compute/core/TensorInfo.h"
#include "arm_compute/core/Types.h"
#include "arm_compute/core/Validate.h"

#include <arm_neon.h>
#include <cstddef>
#include <cstdint>

using namespace arm_compute;

template <typename T>
void NECol2ImKernel::run_col2im(const Window &window)
{
    const int output_stride_x = _output->info()->strides_in_bytes().x();
    const int output_stride_y = _output->info()->strides_in_bytes().y();
    const int output_stride_z = _output->info()->strides_in_bytes().z();

    Window window_out(window);
    window_out.set(Window::DimX, Window::Dimension(0, 0, 0));
    window_out.set(Window::DimY, Window::Dimension(0, 0, 0));
    window_out.set(Window::DimZ, Window::Dimension(0, 0, 0));

    // Create iterators
    Iterator in(_input, window);
    Iterator out(_output, window_out);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const int hidx = id.y();
        const int idx  = id.x() * output_stride_z + (hidx / _convolved_dims.first) * output_stride_y + (hidx % _convolved_dims.first) * output_stride_x;

        *(reinterpret_cast<T *>(out.ptr() + idx)) = *(reinterpret_cast<const T *>(in.ptr()));
    },
    in, out);
}

NECol2ImKernel::NECol2ImKernel()
    : _func(), _input(nullptr), _output(nullptr), _convolved_dims()
{
}

void NECol2ImKernel::configure(const ITensor *input, ITensor *output, std::pair<unsigned int, unsigned int> convolved_dims)
{
    ARM_COMPUTE_ERROR_ON_NULLPTR(input, output);

    set_data_type_if_unknown(*output->info(), input->info()->data_type());

    TensorShape output_shape = input->info()->tensor_shape();
    output_shape.set(0, convolved_dims.first);
    output_shape.set(1, convolved_dims.second);
    output_shape.set(2, input->info()->tensor_shape()[0]);

    set_shape_if_empty(*output->info(), output_shape);

    ARM_COMPUTE_ERROR_ON_MISMATCHING_DIMENSIONS(output->info()->tensor_shape(), output_shape);
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(input, 1, DataType::U8, DataType::S8, DataType::QS8, DataType::U16, DataType::S16, DataType::U32, DataType::S32, DataType::F16, DataType::F32);
    ARM_COMPUTE_ERROR_ON_MISMATCHING_DATA_TYPES(input, output);

    _input          = input;
    _output         = output;
    _convolved_dims = convolved_dims;

    switch(input->info()->element_size())
    {
        case 1:
            _func = &NECol2ImKernel::run_col2im<uint8_t>;
            break;
        case 2:
            _func = &NECol2ImKernel::run_col2im<uint16_t>;
            break;
        case 4:
            _func = &NECol2ImKernel::run_col2im<uint32_t>;
            break;
        default:
            ARM_COMPUTE_ERROR("Element size not supported");
            break;
    }

    // Configure kernel window
    Window win = calculate_max_window(*input->info(), Steps());

    // The NECol2ImKernel doesn't need padding so update_window_and_padding() can be skipped
    Coordinates coord;
    coord.set_num_dimensions(output->info()->num_dimensions());
    output->info()->set_valid_region(ValidRegion(coord, output->info()->tensor_shape()));

    INEKernel::configure(win);
}

void NECol2ImKernel::run(const Window &window)
{
    ARM_COMPUTE_ERROR_ON_UNCONFIGURED_KERNEL(this);
    ARM_COMPUTE_ERROR_ON_INVALID_SUBWINDOW(INEKernel::window(), window);

    (this->*_func)(window);
}
