#include <menu/object/input/Slider.hpp>
#include <menu/Menu.hpp>

namespace zerokernel
{

settings::RVariable<int> SliderStyle::default_width{ "zk.style.input.slider.width", "60" };
settings::RVariable<int> SliderStyle::default_height{ "zk.style.input.slider.height", "14" };

settings::RVariable<int> SliderStyle::handle_width{ "zk.style.input.slider.handle_width", "6" };
settings::RVariable<int> SliderStyle::bar_width{ "zk.style.input.slider.bar_width", "2" };

settings::RVariable<rgba_t> SliderStyle::handle_body{ "zk.style.input.slider.color.handle_body", "ff8800ff" };
settings::RVariable<rgba_t> SliderStyle::handle_border{ "zk.style.input.slider.color.handle_border", "000000ff" };
settings::RVariable<rgba_t> SliderStyle::bar_color{ "zk.style.input.slider.color.bar", "a5a5a525" };
} // namespace zerokernel
