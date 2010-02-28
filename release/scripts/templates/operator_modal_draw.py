import BGL

def draw_callback_px(self, context):
    print("mouse points", len(self.mouse_path))    

    # 50% alpha, 2 pixel width line
    BGL.glEnable(BGL.GL_BLEND)
    BGL.glColor4f(0.0, 0.0, 0.0, 0.5)
    BGL.glLineWidth(2)

    BGL.glBegin(BGL.GL_LINE_STRIP)
    for x, y in self.mouse_path:
        BGL.glVertex2i(x, y)

    BGL.glEnd()

    # restore opengl defaults 
    BGL.glLineWidth(1)
    BGL.glDisable(BGL.GL_BLEND)
    BGL.glColor4f(0.0, 0.0, 0.0, 1.0)


class ModalDrawOperator(bpy.types.Operator):
    '''Draw a line with the mouse'''
    bl_idname = "object.modal_operator"
    bl_label = "Simple Modal Operator"

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type == 'MOUSEMOVE':
            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

        elif event.type == 'LEFTMOUSE':
            context.region.callback_remove(self._handle)
            return {'FINISHED'}

        elif event.type in ('RIGHTMOUSE', 'ESC'):
            context.region.callback_remove(self._handle)
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type == 'VIEW_3D':
            context.manager.add_modal_handler(self)
            
            # Add the region OpenGL drawing callback
            # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
            self._handle = context.region.callback_add(draw_callback_px, (self, context), 'POST_PIXEL')

            self.mouse_path = []

            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot run operator")
            return {'CANCELLED'}


def register():
    bpy.types.register(ModalDrawOperator)


def unregister():
    bpy.types.unregister(ModalDrawOperator)


if __name__ == "__main__":
    register()
