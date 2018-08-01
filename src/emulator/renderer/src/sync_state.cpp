#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <renderer/types.h>

#include <gxm/types.h>
#include <util/log.h>

namespace renderer {
    static GLenum translate_depth_func(SceGxmDepthFunc depth_func) {
        R_PROFILE(__func__);

        switch (depth_func) {
        case SCE_GXM_DEPTH_FUNC_NEVER:
            return GL_NEVER;
        case SCE_GXM_DEPTH_FUNC_LESS:
            return GL_LESS;
        case SCE_GXM_DEPTH_FUNC_EQUAL:
            return GL_EQUAL;
        case SCE_GXM_DEPTH_FUNC_LESS_EQUAL:
            return GL_LEQUAL;
        case SCE_GXM_DEPTH_FUNC_GREATER:
            return GL_GREATER;
        case SCE_GXM_DEPTH_FUNC_NOT_EQUAL:
            return GL_NOTEQUAL;
        case SCE_GXM_DEPTH_FUNC_GREATER_EQUAL:
            return GL_GEQUAL;
        case SCE_GXM_DEPTH_FUNC_ALWAYS:
            return GL_ALWAYS;
        }

        return GL_ALWAYS;
    }

    static GLenum translate_stencil_op(SceGxmStencilOp stencil_op) {
        R_PROFILE(__func__);

        switch (stencil_op) {
        case SCE_GXM_STENCIL_OP_KEEP:
            return GL_KEEP;
        case SCE_GXM_STENCIL_OP_ZERO:
            return GL_ZERO;
        case SCE_GXM_STENCIL_OP_REPLACE:
            return GL_REPLACE;
        case SCE_GXM_STENCIL_OP_INCR:
            return GL_INCR;
        case SCE_GXM_STENCIL_OP_DECR:
            return GL_DECR;
        case SCE_GXM_STENCIL_OP_INVERT:
            return GL_INVERT;
        case SCE_GXM_STENCIL_OP_INCR_WRAP:
            return GL_INCR_WRAP;
        case SCE_GXM_STENCIL_OP_DECR_WRAP:
            return GL_DECR_WRAP;
        }

        return GL_KEEP;
    }

    static GLenum translate_stencil_func(SceGxmStencilFunc stencil_func) {
        R_PROFILE(__func__);

        switch (stencil_func) {
        case SCE_GXM_STENCIL_FUNC_NEVER:
            return GL_NEVER;
        case SCE_GXM_STENCIL_FUNC_LESS:
            return GL_LESS;
        case SCE_GXM_STENCIL_FUNC_EQUAL:
            return GL_EQUAL;
        case SCE_GXM_STENCIL_FUNC_LESS_EQUAL:
            return GL_LEQUAL;
        case SCE_GXM_STENCIL_FUNC_GREATER:
            return GL_GREATER;
        case SCE_GXM_STENCIL_FUNC_NOT_EQUAL:
            return GL_NOTEQUAL;
        case SCE_GXM_STENCIL_FUNC_GREATER_EQUAL:
            return GL_GEQUAL;
        case SCE_GXM_STENCIL_FUNC_ALWAYS:
            return GL_ALWAYS;
        }

        return GL_ALWAYS;
    }

    static void set_stencil_state(GLenum face, const GxmStencilState &state) {
        glStencilOpSeparate(face,
            translate_stencil_op(state.stencil_fail),
            translate_stencil_op(state.depth_fail),
            translate_stencil_op(state.depth_pass));
        glStencilFuncSeparate(face, translate_stencil_func(state.func), state.ref, state.compare_mask);
        glStencilMaskSeparate(face, state.write_mask);
    }

    bool sync_state(Context &context, const GxmContextState &state, const MemState &mem, bool enable_texture_cache) {
        R_PROFILE(__func__);

        // TODO Use some kind of caching to avoid setting every draw call?
        const SharedGLObject program = compile_program(context.program_cache, state, mem);
        if (!program) {
            return false;
        }
        glUseProgram(program->get());

        // Viewport.
        const GLsizei display_w = state.color_surface.pbeEmitWords[0];
        const GLsizei display_h = state.color_surface.pbeEmitWords[1];
        const GxmViewport &viewport = state.viewport;
        if (viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
            const GLfloat w = viewport.scale.x * 2;
            const GLfloat h = viewport.scale.y * -2;
            const GLfloat x = viewport.offset.x - viewport.scale.x;
            const GLfloat y = display_h - (viewport.offset.y - viewport.scale.y);
            glViewportIndexedf(0, x, y, w, h);
            glDepthRange(viewport.offset.z - viewport.scale.z, viewport.offset.z + viewport.scale.z);
        } else {
            glViewport(0, 0, display_w, display_h);
            glDepthRange(0, 1);
        }

        // Clipping.
        // TODO: There was some math here to round the scissor rect, but it looked potentially incorrect.
        const GLsizei scissor_x = state.region_clip_min.x;
        const GLsizei scissor_y = display_h - state.region_clip_max.y - 1;
        const GLsizei scissor_w = state.region_clip_max.x - state.region_clip_min.x + 1;
        const GLsizei scissor_h = state.region_clip_max.y - state.region_clip_min.y + 1;
        switch (state.region_clip_mode) {
        case SCE_GXM_REGION_CLIP_NONE:
            glDisable(GL_SCISSOR_TEST);
            break;
        case SCE_GXM_REGION_CLIP_ALL:
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 0, 0);
            break;
        case SCE_GXM_REGION_CLIP_OUTSIDE:
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
            break;
        case SCE_GXM_REGION_CLIP_INSIDE:
            // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
            glDisable(GL_SCISSOR_TEST);
            LOG_WARN("Unimplemented region clip mode used: SCE_GXM_REGION_CLIP_INSIDE");
            break;
        }

        // Culling.
        switch (state.cull_mode) {
        case SCE_GXM_CULL_CCW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case SCE_GXM_CULL_CW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case SCE_GXM_CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        }

        // Depth test.
        if (state.depth_stencil_surface.depthData) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(translate_depth_func(state.front_depth_func));
            glDepthMask(state.front_depth_write_enable == SCE_GXM_DEPTH_WRITE_ENABLED ? GL_TRUE : GL_FALSE);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        // Stencil.
        if (state.depth_stencil_surface.stencilData) {
            glEnable(GL_STENCIL_TEST);
            set_stencil_state(GL_FRONT, state.front_stencil);
            set_stencil_state(GL_BACK, state.back_stencil);
        } else {
            glDisable(GL_STENCIL_TEST);
        }

        // Blending.
        const FragmentProgram &fragment_program = *state.fragment_program.get(mem)->renderer.get();
        glColorMask(fragment_program.color_mask_red, fragment_program.color_mask_green, fragment_program.color_mask_blue, fragment_program.color_mask_alpha);
        if (fragment_program.blend_enabled) {
            glEnable(GL_BLEND);
            glBlendEquationSeparate(fragment_program.color_func, fragment_program.alpha_func);
            glBlendFuncSeparate(fragment_program.color_src, fragment_program.color_dst, fragment_program.alpha_src, fragment_program.alpha_dst);
        } else {
            glDisable(GL_BLEND);
        }

        // Textures.
        for (size_t i = 0; i < SCE_GXM_MAX_TEXTURE_UNITS; ++i) {
            const SceGxmTexture &texture = state.fragment_textures[i];
            if (texture.data_addr != 0) {
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
                cache_and_bind_texture(context.texture_cache, texture, mem, enable_texture_cache);
            }
        }
        glActiveTexture(GL_TEXTURE0);

        // Uniforms.
        set_uniforms(program->get(), state, mem);

        // Vertex attributes.
        const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
        for (const emu::SceGxmVertexAttribute &attribute : vertex_program.attributes) {
            const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];
            const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
            const GLenum type = attribute_format_to_gl_type(attribute_format);
            const GLboolean normalised = attribute_format_normalised(attribute_format);
            const int attrib_location = attribute.regIndex / sizeof(uint32_t);

            glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[attribute.streamIndex]);
            glVertexAttribPointer(attrib_location, attribute.componentCount, type, normalised, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset));
            glEnableVertexAttribArray(attrib_location);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return true;
    }
}
