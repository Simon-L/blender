# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# Use for validating our wiki interlinking.
#  ./blender.bin --background -noaudio --python source/tests/bl_rna_wiki_reference.py
#
# 1) test_lookup_coverage()   -- ensure that we have lookups for _every_ RNA path
# 2) test_urls()              -- ensure all the URL's are correct
# 3) test_language_coverage() -- ensure language lookup table is complete
#

import bpy

# a stripped down version of api_dump() in rna_info_dump.py


def test_lookup_coverage():

    def rna_ids():
        
        import rna_info
        struct = rna_info.BuildRNAInfo()[0]
        for struct_id, v in sorted(struct.items()):
            props = [(prop.identifier, prop) for prop in v.properties]
            for prop_id, prop in props:
                yield "bpy.types.%s.%s" % (struct_id[1], prop_id)

        for submod_id in dir(bpy.ops):
            for op_id in dir(getattr(bpy.ops, submod_id)):
                yield "bpy.ops.%s.%s" % (submod_id, op_id)

    # check coverage
    from bl_operators import wm

    for rna_id in rna_ids():
        url = wm.WM_OT_doc_view_manual._lookup_rna_url(rna_id, verbose=False)
        print(rna_id, "->", url)


def test_urls():
    pass  # TODO


def test_language_coverage():
    pass  # TODO


def main():
    test_lookup_coverage()
    test_language_coverage()

if __name__ == "__main__":
    main()
