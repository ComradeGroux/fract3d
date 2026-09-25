BOLD	:= \033[1m
GRAY	:= \033[90m
GREEN	:= \033[32m
BLUE	:= \033[34m
RESET	:= \033[0m
ERASE	:= \r\033[2K

TARGET	:= fract3d

SRC_DIR			:= src
INCLUDE_DIR 	:= include
BUILD_DIR		:= build
OBJS_DIR		:= $(BUILD_DIR)/objs
DEPS_DIR		:= lib
LIB_SEP			:= $(BUILD_DIR)/.lib_sep
SHADER_DIR		:= shader
SPVS_DIR		:= $(BUILD_DIR)/shaders-spv
SPVS_HEADER_DIR := $(BUILD_DIR)/shaders-include

SRCS	:= $(wildcard $(SRC_DIR)/*.cpp)
VPATH	:= $(dir $(SRCS))
OBJS	:= $(addprefix $(OBJS_DIR)/, $(notdir $(SRCS:.cpp=.o)))
SHADER_SRCS :=	base.vert 		\
				mandelbox.frag
SHADER_SPVS := $(patsubst $(SHADER_DIR)/%, $(SPVS_DIR)/%.spv, $(addprefix $(SHADER_DIR)/, $(SHADER_SRCS)))


VOLK_VER			:= 1.3.295
VOLK_URL			:= https://github.com/zeux/volk/archive/refs/tags/$(VOLK_VER).tar.gz
VOLK_DEP_DIR		:= $(DEPS_DIR)/volk-$(VOLK_VER)


VMA_VER				:= 3.3.0
VMA_URL				:= https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v$(VMA_VER).tar.gz
VMA_DEP_DIR			:= $(DEPS_DIR)/VulkanMemoryAllocator-$(VMA_VER)


GLM_VER				:= 1.0.3
GLM_URL				:= https://github.com/g-truc/glm/archive/refs/tags/$(GLM_VER).tar.gz
GLM_DEP_DIR			:= $(DEPS_DIR)/glm-$(GLM_VER)


SDL_VER				:= 3.4.2
SDL_URL				:= https://github.com/libsdl-org/SDL/archive/refs/tags/release-$(SDL_VER).tar.gz
SDL_DEP_DIR			:= $(DEPS_DIR)/sdl-$(SDL_VER)
SDL_BUILD_DIR		:= $(BUILD_DIR)/sdl
SDL_LIB				:= $(SDL_BUILD_DIR)/libSDL3.a


SHADERC_VER			:= 2026.1
SHADERC_URL			:= https://github.com/google/shaderc/archive/refs/tags/v$(SHADERC_VER).tar.gz
SHADERC_DEP_DIR		:= $(DEPS_DIR)/shaderc-$(SHADERC_VER)
SHADERC_BUILD_DIR	:= $(BUILD_DIR)/shaderc

GLSLC_LOCAL			:= $(SHADERC_BUILD_DIR)/glslc/glslc
GLSLC_SYSTEM		:= $(shell which glslc 2>/dev/null)

SPIRV_TOOLS_REV		:= fbe4f3ad913c44fe8700545f8ffe35d1382b7093
SPIRV_TOOLS_URL		:= https://github.com/KhronosGroup/SPIRV-Tools/archive/$(SPIRV_TOOLS_REV).tar.gz

SPIRV_HEADERS_REV	:= 04f10f650d514df88b76d25e83db360142c7b174
SPIRV_HEADERS_URL	:= https://github.com/KhronosGroup/SPIRV-Headers/archive/$(SPIRV_HEADERS_REV).tar.gz

GLSLANG_REV			:= f0bd0257c308b9a26562c1a30c4748a0219cc951
GLSLANG_URL			:= https://github.com/KhronosGroup/glslang/archive/$(GLSLANG_REV).tar.gz

CXX 		:= g++
LDFLAGS 	:= -rdynamic
CXXFLAGS 	:= -std=c++17 -O2 -Wall -Wextra -Werror
INCLUDES	:= -I$(INCLUDE_DIR) \
			   -I$(GLM_DEP_DIR) \
			   -I$(SDL_DEP_DIR)/include \
			   -I$(VOLK_DEP_DIR) \
			   -I../include \
			   -isystem$(VMA_DEP_DIR)/include
LDFLAGS		:= -lm -ldl -lpthread -lwayland-client -lwayland-egl -lwayland-cursor -lxkbcommon -ldecor-0
DEBUG_FLAG	:= -DDEBUG

ifdef GLSLC_SYSTEM
  GLSLC	:= $(GLSLC_SYSTEM)
else
  GLSLC	:= $(GLSLC_LOCAL)
endif

all: $(TARGET)

debug: CXXFLAGS += ${DEBUG_FLAG}
debug: clean all

$(TARGET): $(SDL_LIB) $(SHADER_SPVS) $(OBJS)
	@printf "$(BOLD)Linking $(TARGET)$(RESET)\n"
	@$(CXX) $(OBJS) $(SDL_LIB) $(LDFLAGS) -o $@
	@printf "$(GREEN)  ✓ $(TARGET) ready$(RESET)\n"
	@rm -f $(LIB_SEP)

$(OBJS_DIR):
	@mkdir -p $@
$(OBJS):  | $(VOLK_DEP_DIR) $(VMA_DEP_DIR) $(GLM_DEP_DIR) $(SDL_DEP_DIR) $(OBJS_DIR)/.compile_start
$(OBJS_DIR)/.fractal_start: $(SRCS) | $(OBJS_DIR)
	@test -f $(LIB_SEP) && printf "\n" || mkdir -p $(BUILD_DIR); touch $(LIB_SEP)
	@printf "$(BOLD)$(BLUE)Compiling $(TARGET)$(RESET)\n"
	@touch $@
$(OBJS_DIR)/.compile_start: $(SRCS) | $(OBJS_DIR)/.fractal_start
	@printf "$(BOLD)Compiling$(RESET)\n"
	@touch $@
$(OBJS_DIR)/%.o: %.cpp | $(OBJS_DIR)
	@printf "$(GRAY)  $<...$(RESET)" && \
	 $(CXX) $(CXXFLAGS) $(INCLUDES) -I$(SPVS_HEADER_DIR) -c $< -o $@ && \
	 printf "$(ERASE)$(GREEN)  ✓ $<$(RESET)\n"\

$(SPVS_DIR):
	@mkdir -p $@
$(SPVS_HEADER_DIR):
	@mkdir -p $@
	@printf "$(BOLD)Compiling SPIRV shaders$(RESET)\n"
$(SPVS_DIR)/%.spv: $(SHADER_DIR)/% | glslc_ready $(SPVS_DIR) $(SPVS_HEADER_DIR)
	@printf "$(GRAY)  Compiling shader $<...$(RESET)" && \
	 $(GLSLC) $< -o $@ && \
	 xxd -i $@ > $(addsuffix ".h", $(addprefix $(SPVS_HEADER_DIR)/, $(notdir $<))) && \
	 printf "$(ERASE)$(GREEN)  ✓ $<$(RESET)\n"

glslc_ready:
ifdef GLSLC_SYSTEM
	@printf "$(GREEN)  ✓ glslc found: $(GLSLC_SYSTEM)$(RESET)\n"
else
	@if [ ! -x "$(GLSLC_LOCAL)" ]; then \
		$(MAKE) $(GLSLC_LOCAL) --no-print-directory; \
	fi
endif

$(DEPS_DIR):
	@mkdir -p $@

$(VOLK_DEP_DIR): | $(DEPS_DIR)
	@printf "$(BOLD)Downloading VOLK $(VOLK_VER)$(RESET)\n"
	@printf "$(GRAY)  Fetching archive...$(RESET)" && \
	 curl -sL $(VOLK_URL) -o $(DEPS_DIR)/volk.tar.gz && \
	 printf "$(ERASE)"
	@printf "$(GRAY)  Extracting...$(RESET)" && \
	 tar -xzf $(DEPS_DIR)/volk.tar.gz -C $(DEPS_DIR) && \
	 printf "$(ERASE)"
	@rm $(DEPS_DIR)/volk.tar.gz
	@printf "$(GREEN)  ✓ Done$(RESET)\n"

$(VMA_DEP_DIR): | $(DEPS_DIR)
	@printf "$(BOLD)Downloading VMA $(VMA_VER)$(RESET)\n"
	@printf "$(GRAY)  Fetching archive...$(RESET)" && \
	 curl -sL $(VMA_URL) -o $(DEPS_DIR)/vma.tar.gz && \
	 printf "$(ERASE)"
	@printf "$(GRAY)  Extracting...$(RESET)" && \
	 tar -xzf $(DEPS_DIR)/vma.tar.gz -C $(DEPS_DIR) && \
	 printf "$(ERASE)"
	@rm $(DEPS_DIR)/vma.tar.gz
	@printf "$(GREEN)  ✓ Done$(RESET)\n"

$(GLM_DEP_DIR): | $(DEPS_DIR)
	@printf "$(BOLD)Downloading GLM $(GLM_VER)$(RESET)\n"
	@printf "$(GRAY)  Fetching archive...$(RESET)" && \
	 curl -sL $(GLM_URL) -o $(DEPS_DIR)/glm.tar.gz && \
	 printf "$(ERASE)"
	@printf "$(GRAY)  Extracting...$(RESET)" && \
	 tar -xzf $(DEPS_DIR)/glm.tar.gz -C $(DEPS_DIR) && \
	 printf "$(ERASE)"
	@rm $(DEPS_DIR)/glm.tar.gz
	@printf "$(GREEN)  ✓ Done$(RESET)\n"

$(SDL_DEP_DIR): | $(DEPS_DIR)
	@printf "$(BOLD)Downloading SDL $(SDL_VER)$(RESET)\n"
	@printf "$(GRAY)  Fetching archive...$(RESET)" && \
	 curl -sL $(SDL_URL) -o $(DEPS_DIR)/sdl.tar.gz && \
	 printf "$(ERASE)"
	@mkdir -p $(SDL_DEP_DIR)
	@printf "$(GRAY)  Extracting...$(RESET)" && \
	 tar -xzf $(DEPS_DIR)/sdl.tar.gz -C $(SDL_DEP_DIR) --strip-components=1 && \
	 printf "$(ERASE)"
	@rm $(DEPS_DIR)/sdl.tar.gz
	@printf "$(GREEN)  ✓ Done$(RESET)\n"
$(SDL_LIB) : $(SDL_DEP_DIR) | $(OBJS_DIR)
	@printf "$(BOLD)Building SDL $(SDL_VER)$(RESET)\n"
	@mkdir -p $(SDL_BUILD_DIR)
	@printf "$(GRAY)  Configuring cmake... (can take several minutes)$(RESET)" && \
	 cmake -S $(SDL_DEP_DIR) -B $(SDL_BUILD_DIR) \
		-DSDL_STATIC=ON                     \
		-DSDL_SHARED=OFF                    \
		-DSDL_DEPS_SHARED=OFF               \
		-DSDL_VIDEO=ON                      \
		-DSDL_EVENTS=ON                     \
		-DSDL_VULKAN=ON                     \
		-DSDL_TIMERS=ON                     \
		-DSDL_PTHREADS=ON                   \
		-DSDL_PTHREADS_SEM=ON               \
		-DSDL_GCC_ATOMICS=ON                \
		-DSDL_LIBC=ON                       \
		-DSDL_CLOCK_GETTIME=ON              \
		-DSDL_WAYLAND=ON                    \
		-DSDL_WAYLAND_SHARED=ON            \
		-DSDL_WAYLAND_LIBDECOR=ON          \
		-DSDL_WAYLAND_LIBDECOR_SHARED=ON   \
		-DSDL_X11=OFF                       \
		-DSDL_AUDIO=OFF                     \
		-DSDL_ALSA=OFF                      \
		-DSDL_ALSA_SHARED=OFF               \
		-DSDL_JACK=OFF                      \
		-DSDL_JACK_SHARED=OFF               \
		-DSDL_PIPEWIRE=OFF                  \
		-DSDL_PIPEWIRE_SHARED=OFF           \
		-DSDL_PULSEAUDIO=OFF                \
		-DSDL_PULSEAUDIO_SHARED=OFF         \
		-DSDL_SNDIO=OFF                     \
		-DSDL_SNDIO_SHARED=OFF              \
		-DSDL_DISKAUDIO=OFF                 \
		-DSDL_DUMMYAUDIO=OFF                \
		-DSDL_CAMERA=OFF                    \
		-DSDL_DUMMYCAMERA=OFF               \
		-DSDL_RENDER=OFF                    \
		-DSDL_RENDER_GPU=OFF                \
		-DSDL_RENDER_VULKAN=OFF             \
		-DSDL_GPU=OFF                       \
		-DSDL_OPENGL=OFF                    \
		-DSDL_OPENGLES=OFF                  \
		-DSDL_OFFSCREEN=OFF                 \
		-DSDL_DUMMYVIDEO=OFF                \
		-DSDL_KMSDRM=OFF                    \
		-DSDL_KMSDRM_SHARED=OFF             \
		-DSDL_JOYSTICK=OFF                  \
		-DSDL_HIDAPI=OFF                    \
		-DSDL_HIDAPI_JOYSTICK=OFF           \
		-DSDL_HIDAPI_LIBUSB=OFF             \
		-DSDL_HIDAPI_LIBUSB_SHARED=OFF      \
		-DSDL_VIRTUAL_JOYSTICK=OFF          \
		-DSDL_HAPTIC=OFF                    \
		-DSDL_SENSOR=OFF                    \
		-DSDL_POWER=OFF                     \
		-DSDL_DIALOG=OFF                    \
		-DSDL_TRAY=OFF                      \
		-DSDL_DBUS=OFF                      \
		-DSDL_IBUS=OFF                      \
		-DSDL_LIBURING=OFF                  \
		-DSDL_LIBUDEV=OFF                   \
		-DSDL_FRIBIDI=OFF                   \
		-DSDL_FRIBIDI_SHARED=OFF            \
		-DSDL_LIBTHAI=OFF                   \
		-DSDL_LIBTHAI_SHARED=OFF            \
		-DSDL_ASSEMBLY=OFF                  \
		-DSDL_INSTALL=OFF                   \
		-DSDL_INSTALL_CPACK=OFF             \
		-DSDL_UNINSTALL=OFF                 \
		-DSDL_TESTS=OFF                     \
		-DSDL_TEST_LIBRARY=OFF              \
		-DSDL_EXAMPLES=OFF                  \
		-DSDL_ASAN=OFF                      \
		-DSDL_WERROR=OFF                    \
		-DSDL_CCACHE=OFF                    \
		-DSDL_CLANG_TIDY=OFF                \
		-DSDL_DLOPEN_NOTES=OFF              \
		-DSDL_RPATH=OFF						\
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON\
		-DCMAKE_BUILD_TYPE=Release 			\
	 > /dev/null 2>&1 && \
	 printf "$(ERASE)"
	@printf "$(GRAY)  Compiling... (can take several minutes)$(RESET)" && \
	 $(MAKE) -C $(SDL_BUILD_DIR) -j$(shell nproc) --no-print-directory > /dev/null 2>&1 && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Done$(RESET)\n"


$(SHADERC_DEP_DIR): | $(DEPS_DIR)
	@printf "$(BOLD)Downloading SHADERC $(SHADERC_VER)$(RESET)\n"
	@printf "$(GRAY)  Fetching shaderc...$(RESET)" && \
	 curl -sL $(SHADERC_URL) -o $(DEPS_DIR)/shaderc.tar.gz && \
	 tar -xzf $(DEPS_DIR)/shaderc.tar.gz -C $(DEPS_DIR) && \
	 rm $(DEPS_DIR)/shaderc.tar.gz && \
	 printf "$(ERASE)$(GREEN)  ✓ Done$(RESET)\n"

	@printf "$(BOLD)Downloading SHADERC's dependencies$(RESET)\n"
	@printf "$(GRAY)  Fetching glslang...$(RESET)" && \
	 curl -sL $(GLSLANG_URL) -o $(DEPS_DIR)/glslang.tar.gz && \
	 tar -xzf $(DEPS_DIR)/glslang.tar.gz -C $(DEPS_DIR) && \
	 mv $(DEPS_DIR)/glslang-$(GLSLANG_REV) $(SHADERC_DEP_DIR)/third_party/glslang && \
	 rm $(DEPS_DIR)/glslang.tar.gz && \
	 printf "$(ERASE)$(GREEN)  ✓ glslang$(RESET)\n"

	@printf "$(GRAY)  Fetching SPIRV-Tools...$(RESET)" && \
	 curl -sL $(SPIRV_TOOLS_URL) -o $(DEPS_DIR)/spirv-tools.tar.gz && \
	 tar -xzf $(DEPS_DIR)/spirv-tools.tar.gz -C $(DEPS_DIR) && \
	 mv $(DEPS_DIR)/SPIRV-Tools-$(SPIRV_TOOLS_REV) $(SHADERC_DEP_DIR)/third_party/spirv-tools && \
	 rm $(DEPS_DIR)/spirv-tools.tar.gz && \
	 printf "$(ERASE)$(GREEN)  ✓ spirv-tools$(RESET)\n"

	@printf "$(GRAY)  Fetching SPIRV-Headers...$(RESET)" && \
	 curl -sL $(SPIRV_HEADERS_URL) -o $(DEPS_DIR)/spirv-headers.tar.gz && \
	 tar -xzf $(DEPS_DIR)/spirv-headers.tar.gz -C $(DEPS_DIR) && \
	 mv $(DEPS_DIR)/SPIRV-Headers-$(SPIRV_HEADERS_REV) $(SHADERC_DEP_DIR)/third_party/spirv-headers && \
	 rm $(DEPS_DIR)/spirv-headers.tar.gz && \
	 printf "$(ERASE)$(GREEN)  ✓ spirv-headers$(RESET)\n"


$(GLSLC_LOCAL): $(SHADERC_DEP_DIR)
	@printf "$(BOLD)Building SHADERC $(SHADERC_VER)$(RESET)\n"
	@mkdir -p $(SHADERC_BUILD_DIR)
	@printf "$(GRAY)  Configuring cmake...$(RESET)" && \
	 cmake -S $(SHADERC_DEP_DIR) -B $(SHADERC_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=Release        \
		-DSHADERC_SKIP_TESTS=ON           \
		-DSHADERC_SKIP_EXAMPLES=ON        \
		-DSHADERC_SKIP_COPYRIGHT_CHECK=ON \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		> /dev/null 2>&1 && \
	 printf "$(ERASE)"
	@printf "$(GRAY)  Compiling SHADERC... (can take several minutes)$(RESET)" && \
	 $(MAKE) -C $(SHADERC_BUILD_DIR) glslc_exe -j$(shell nproc) --no-print-directory > /dev/null 2>&1 && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Done$(RESET)\n"


clean:
	@printf "$(BOLD)$(BLUE)Cleaning $(TARGET) objects...$(RESET)\n"
	@printf "$(GRAY)  Removing build objects...$(RESET)" && \
	 rm -rf $(OBJS_DIR) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Build files cleaned$(RESET)\n"

fclean:
	@printf "$(BOLD)$(BLUE)Cleaning $(TARGET)...$(RESET)\n"
	@printf "$(GRAY)  Removing $(BUILD_DIR)...$(RESET)" && \
	 rm -rf $(BUILD_DIR) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Build files cleaned$(RESET)\n"
	@printf "$(GRAY)  Removing $(TARGET)...$(RESET)" && \
	 rm -rf $(TARGET) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ $(TARGET) cleaned$(RESET)\n"

dclean:
	@printf "$(BOLD)$(BLUE)Cleaning $(TARGET)...$(RESET)\n"
	@printf "$(GRAY)  Removing $(BUILD_DIR)...$(RESET)" && \
	 rm -rf $(BUILD_DIR) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Build files cleaned$(RESET)\n"
	@printf "$(GRAY)  Removing $(TARGET)...$(RESET)" && \
	 rm -rf $(TARGET) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ $(TARGET) cleaned$(RESET)\n"
	@printf "$(GRAY)  Removing $(DEPS_DIR)...$(RESET)" && \
	 rm -rf $(DEPS_DIR) && \
	 printf "$(ERASE)"
	@printf "$(GREEN)  ✓ Dependencies cleaned$(RESET)\n"

seperate:
	@printf	"\n"
re: fclean seperate all

.PHONY: all clean fclean dclean re
