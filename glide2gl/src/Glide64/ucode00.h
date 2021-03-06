/*
* Glide64 - Glide video plugin for Nintendo 64 emulators.
* Copyright (c) 2002  Dave2001
* Copyright (c) 2003-2009  Sergey 'Gonetz' Lipski
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//****************************************************************
//
// Glide64 - Glide Plugin for Nintendo 64 emulators
// Project started on December 29th, 2001
//
// Authors:
// Dave2001, original author, founded the project in 2001, left it in 2002
// Gugaman, joined the project in 2002, left it in 2002
// Sergey 'Gonetz' Lipski, joined the project in 2002, main author since fall of 2002
// Hiroshi 'KoolSmoky' Morii, joined the project in 2007
//
//****************************************************************
//
// To modify Glide64:
// * Write your name and (optional)email, commented by your work, so I know who did it, and so that you can find which parts you modified when it comes time to send it to me.
// * Do NOT send me the whole project or file that you modified.  Take out your modified code sections, and tell me where to put them.  If people sent the whole thing, I would have many different versions, but no idea how to combine them all.
//
//****************************************************************
#include "GBI.h"

static void rsp_vertex(int v0, int n)
{
   unsigned int i;
   uint32_t addr = segoffset(rdp.cmd1) & 0x00FFFFFF;

   // This is special, not handled in update(), but here
   // * Matrix Pre-multiplication idea by Gonetz (Gonetz@ngs.ru)
   if (rdp.update & UPDATE_MULT_MAT)
   {
      rdp.update ^= UPDATE_MULT_MAT;
      MulMatrices(rdp.model, rdp.proj, rdp.combined);
   }

   // This is special, not handled in update()
   if (rdp.update & UPDATE_LIGHTS)
   {
      rdp.update ^= UPDATE_LIGHTS;

      // Calculate light vectors
      for (i = 0; i < rdp.num_lights; i++)
      {
         InverseTransformVector(&rdp.light[i].dir[0], rdp.light_vector[i], rdp.model);
         NormalizeVector (rdp.light_vector[i]);
      }
   }

   gSPVertex(addr, n, v0);
}

//
// uc0:vertex - loads vertices
//
static void uc0_vertex(uint32_t w0, uint32_t w1)
{
   int v0 = (w0 >> 16) & 0xF; // Current vertex
   int n = ((w0 >> 20) & 0xF) + 1; // Number of vertices to copy
   rsp_vertex(v0, n);
}

// ** Definitions **

static void modelview_load (float m[4][4])
{
   CopyMatrix(rdp.model, m, 64); // 4*4*4 (float)
   rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

static void modelview_mul (float m[4][4])
{
   MulMatrices(m, rdp.model, rdp.model);
   rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

static void modelview_push(void)
{
   if (rdp.model_i == rdp.model_stack_size)
      return;

   CopyMatrix(rdp.model_stack[rdp.model_i++], rdp.model, 64);
}

void modelview_pop (int num)
{
   if (rdp.model_i > num - 1)
      rdp.model_i -= num;
   else
      return;

   memcpy (rdp.model, rdp.model_stack[rdp.model_i], 64);
   rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

static void modelview_load_push (float m[4][4])
{
   modelview_push();
   modelview_load(m);
}

static void modelview_mul_push (float m[4][4])
{
   modelview_push();
   modelview_mul (m);
}

static void projection_load (float m[4][4])
{
   CopyMatrix(rdp.proj, m, 64); // 4*4*4 (float)
   rdp.update |= UPDATE_MULT_MAT;
}

static void projection_mul (float m[4][4])
{
   MulMatrices(m, rdp.proj, rdp.proj);
   rdp.update |= UPDATE_MULT_MAT;
}

static void load_matrix (float m[4][4], uint32_t addr)
{
   //FRDP ("matrix - addr: %08lx\n", addr);
   int x,y;  // matrix index
   uint16_t *src;
   addr >>= 1;
   src = (uint16_t*)gfx_info.RDRAM;

   // Adding 4 instead of one, just to remove mult. later
   for (x = 0; x < 16; x += 4)
   {
      for (y=0; y<4; y++)
         m[x>>2][y] = (float)((((int32_t)src[(addr+x+y)^1]) << 16) | src[(addr+x+y+16)^1]) / 65536.0f;
   }
}

//
// uc0:matrix - performs matrix operations
//
static void uc0_matrix(uint32_t w0, uint32_t w1)
{
   //LRDP("uc0:matrix ");
   DECLAREALIGN16VAR(m[4][4]);
   // Use segment offset to get the address
   uint32_t addr = RSP_SegmentToPhysical(w1);
   uint8_t command = (uint8_t)((w0 >> 16) & 0xFF);

   load_matrix(m, addr);

   switch (command)
   {
      case G_MTX_NOPUSH: // modelview mul nopush
         //LRDP("modelview mul\n");
         modelview_mul (m);
         break;

      case 1: // projection mul nopush
      case 5: // projection mul push, can't push projection
         //LRDP("projection mul\n");
         projection_mul(m);
         break;

      case G_MTX_LOAD: // modelview load nopush
         //LRDP("modelview load\n");
         modelview_load(m);
         break;

      case 3: // projection load nopush
      case 7: // projection load push, can't push projection
         //LRDP("projection load\n");
         projection_load(m);
         break;

      case 4: // modelview mul push
         //LRDP("modelview mul push\n");
         modelview_mul_push(m);
         break;

      case 6: // modelview load push
         //LRDP("modelview load push\n");
         modelview_load_push(m);
         break;
#if 0
      default:
         FRDP_E ("Unknown matrix command, %02lx", command);
         FRDP ("Unknown matrix command, %02lx", command);
#endif
   }

   //FRDP ("{%f,%f,%f,%f}\n", m[0][0], m[0][1], m[0][2], m[0][3]);
   //FRDP ("{%f,%f,%f,%f}\n", m[1][0], m[1][1], m[1][2], m[1][3]);
   //FRDP ("{%f,%f,%f,%f}\n", m[2][0], m[2][1], m[2][2], m[2][3]);
   //FRDP ("{%f,%f,%f,%f}\n", m[3][0], m[3][1], m[3][2], m[3][3]);
   //FRDP ("\nmodel\n{%f,%f,%f,%f}\n", rdp.model[0][0], rdp.model[0][1], rdp.model[0][2], rdp.model[0][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.model[1][0], rdp.model[1][1], rdp.model[1][2], rdp.model[1][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.model[2][0], rdp.model[2][1], rdp.model[2][2], rdp.model[2][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.model[3][0], rdp.model[3][1], rdp.model[3][2], rdp.model[3][3]);
   //FRDP ("\nproj\n{%f,%f,%f,%f}\n", rdp.proj[0][0], rdp.proj[0][1], rdp.proj[0][2], rdp.proj[0][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.proj[1][0], rdp.proj[1][1], rdp.proj[1][2], rdp.proj[1][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.proj[2][0], rdp.proj[2][1], rdp.proj[2][2], rdp.proj[2][3]);
   //FRDP ("{%f,%f,%f,%f}\n", rdp.proj[3][0], rdp.proj[3][1], rdp.proj[3][2], rdp.proj[3][3]);
}


//
// uc0:movemem - loads a structure with data
//
static void uc0_movemem(uint32_t w0, uint32_t w1)
{
   //LRDP("uc0:movemem ");

   uint32_t index = (w0 >> 16) & 0xFF;
   int16_t *rdram     = (int16_t*)(gfx_info.RDRAM  + RSP_SegmentToPhysical(w1));
   int8_t  *rdram_s8  = (int8_t*) (gfx_info.RDRAM  + RSP_SegmentToPhysical(w1));
   uint8_t *rdram_u8  = (uint8_t*)(gfx_info.RDRAM  + RSP_SegmentToPhysical(w1));

   // Check the command
   switch (_SHIFTR( w0, 16, 8))
   {
      case F3D_MV_VIEWPORT:
         {
            int16_t scale_y = rdram[0] / 4;
            int16_t scale_x = rdram[1] / 4;
            int16_t scale_z = rdram[3];
            int16_t trans_x = rdram[5] / 4;
            int16_t trans_y = rdram[4] / 4;
            int16_t trans_z = rdram[7];
            if (settings.correct_viewport)
            {
               scale_x = abs(scale_x);
               scale_y = abs(scale_y);
            }
            rdp.view_scale[0] = scale_x * rdp.scale_x;
            rdp.view_scale[1] = -scale_y * rdp.scale_y;
            rdp.view_scale[2] = 32.0f * scale_z;
            rdp.view_trans[0] = trans_x * rdp.scale_x;
            rdp.view_trans[1] = trans_y * rdp.scale_y;
            rdp.view_trans[2] = 32.0f * trans_z;

            rdp.update |= UPDATE_VIEWPORT;
         }
         break;
      case G_MV_LOOKATY:
         {
            int8_t dir_x = rdram_s8[11];
            int8_t dir_y = rdram_s8[10];
            int8_t dir_z = rdram_s8[9];
            rdp.lookat[1][0] = (float)(dir_x) / 127.0f;
            rdp.lookat[1][1] = (float)(dir_y) / 127.0f;
            rdp.lookat[1][2] = (float)(dir_z) / 127.0f;
            rdp.use_lookat = true;
            if (!dir_x && !dir_y)
               rdp.use_lookat = false;
            //FRDP("lookat_y (%f, %f, %f)\n", rdp.lookat[1][0], rdp.lookat[1][1], rdp.lookat[1][2]);
         }
         break;

      case G_MV_LOOKATX:
         {
            rdp.lookat[0][0] = (float)rdram_s8[11] / 127.0f;
            rdp.lookat[0][1] = (float)rdram_s8[10] / 127.0f;
            rdp.lookat[0][2] = (float)rdram_s8[9] / 127.0f;
            rdp.use_lookat = true;
            //FRDP("lookat_x (%f, %f, %f)\n", rdp.lookat[1][0], rdp.lookat[1][1], rdp.lookat[1][2]);
         }
         break;

      case G_MV_L0:
      case G_MV_L1:
      case G_MV_L2:
      case G_MV_L3:
      case G_MV_L4:
      case G_MV_L5:
      case G_MV_L6:
      case G_MV_L7:
         {
            // Get the light #
            uint32_t n = (index - 0x86) >> 1;

            // Get the data
            rdp.light[n].col[0] = (float)rdram_u8[3] / 255.0f;
            rdp.light[n].col[1] = (float)rdram_u8[2] / 255.0f;
            rdp.light[n].col[2] = (float)rdram_u8[1] / 255.0f;
            rdp.light[n].col[3] = 1.0f;

            // ** Thanks to Icepir8 for pointing this out **
            // Lighting must be signed byte instead of byte
            rdp.light[n].dir[0] = (float)rdram_s8[11] / 127.0f;
            rdp.light[n].dir[1] = (float)rdram_s8[10] / 127.0f;
            rdp.light[n].dir[2] = (float)rdram_s8[9] / 127.0f;
            // **

            //rdp.update |= UPDATE_LIGHTS;
         }
         break;


      case G_MV_MATRIX_1:
         {
            uint32_t addr = segoffset(w1) & 0x00FFFFFF;
            // do not update the combined matrix!
            rdp.update &= ~UPDATE_MULT_MAT;

            load_matrix(rdp.combined, addr);

            addr = rdp.pc[rdp.pc_i] & BMASK;
            rdp.pc[rdp.pc_i] = (addr+24) & BMASK; //skip next 3 command, b/c they all are part of gSPForceMatrix
         }
         break;
         //next 3 command should never appear since they will be skipped in previous command
   }
}

//
// uc0:displaylist - makes a call to another section of code
//

static void uc0_displaylist(uint32_t w0, uint32_t w1)
{
   uint32_t addr = segoffset(w1) & 0x00FFFFFF;
   uint32_t push = (w0 >> 16) & 0xFF; // push the old location?

   // This fixes partially Gauntlet: Legends
   if (addr == rdp.pc[rdp.pc_i] - 8)
      return;

   switch (push)
   {
      case 0: // push
         if (rdp.pc_i >= 9)
            return; /* DL stack overflow */
         rdp.pc_i++; // go to the next PC in the stack
         rdp.pc[rdp.pc_i] = addr; // jump to the address
         break;
      case 1: // no push
         rdp.pc[rdp.pc_i] = addr; // jump to the address
         break;
   }
}

//
// tri1 - renders a triangle
//
static void uc0_tri1(uint32_t w0, uint32_t w1)
{
   VERTEX *v[3];

   v[0] = &rdp.vtx[((w1 >> 16) & 0xFF) / 10];
   v[1] = &rdp.vtx[((w1 >> 8) & 0xFF) / 10];
   v[2] = &rdp.vtx[(w1 & 0xFF) / 10];

   cull_trianglefaces(v, 1, true, true, 0);
}

static void uc0_tri1_mischief(uint32_t w0, uint32_t w1)
{
   VERTEX *v[3];

   v[0] = &rdp.vtx[((w1 >> 16) & 0xFF) / 10];
   v[1] = &rdp.vtx[((w1 >> 8) & 0xFF) / 10];
   v[2] = &rdp.vtx[(w1 & 0xFF) / 10];

   {
      int i;
      rdp.force_wrap = false;
      for (i = 0; i < 3; i++)
      {
         if (v[i]->ou < 0.0f || v[i]->ov < 0.0f)
         {
            rdp.force_wrap = true;
            break;
         }
      }
   }

   cull_trianglefaces(v, 1, true, true, 0);
}

//
// uc0:enddl - ends a call made by uc0:displaylist
//
static void uc0_enddl(uint32_t w0, uint32_t w1)
{
   //LRDP("uc0:enddl\n");

   if (rdp.pc_i == 0)
   {
      //LRDP("RDP end\n");

      // Halt execution here
      rdp.halt = 1;
   }

   rdp.pc_i --;
}

static void uc0_culldl(uint32_t w0, uint32_t w1)
{
   VERTEX *v;
   uint16_t i;
   uint8_t v0 = (uint8_t)((w0 & 0x00FFFFFF) / 40) & 0xF;
   uint8_t n = (uint8_t)(w1 / 40) & 0x0F;
   uint32_t cond = 0;

   //FRDP("uc0:culldl start: %d, end: %d\n", v0, n);

   if (n < v0)
      return;
   for (i = v0; i <= n; i++)
   {
      v = &rdp.vtx[i];
      // Check if completely off the screen (quick frustrum clipping for 90 FOV)
      if (v->x >= -v->w)
         cond |= 0x01;
      if (v->x <= v->w)
         cond |= 0x02;
      if (v->y >= -v->w)
         cond |= 0x04;
      if (v->y <= v->w)
         cond |= 0x08;
      if (v->w >= 0.1f)
         cond |= 0x10;

      if (cond == 0x1F)
         return;
   }

   //LRDP(" - "); // specify that the enddl is not a real command
   uc0_enddl(w0, w1);
}

static void uc0_popmatrix(uint32_t w0, uint32_t w1)
{
   //LRDP("uc0:popmatrix\n");

#if 0
   switch (w1)
   {
      case 0: // modelview
         modelview_pop(1);
         break;
      case 1: // projection, can't
         break;

      default:
         FRDP_E ("Unknown uc0:popmatrix command: 0x%08lx\n", w1);
         FRDP ("Unknown uc0:popmatrix command: 0x%08lx\n", w1);
   }
#else
   if (w1 == 0)
      modelview_pop(1);
#endif
}

static void uc0_modifyvtx(uint8_t where, uint16_t vtx, uint32_t val)
{
   VERTEX *v = (VERTEX*)&rdp.vtx[vtx];

   switch (where)
   {
      case 0:
         uc6_obj_sprite(rdp.cmd0, rdp.cmd1);
         break;

      case 0x10: // RGBA
         v->r = (uint8_t)(val >> 24);
         v->g = (uint8_t)((val >> 16) & 0xFF);
         v->b = (uint8_t)((val >> 8) & 0xFF);
         v->a = (uint8_t)(val & 0xFF);
         v->shade_mod = 0;

         //FRDP ("RGBA: %d, %d, %d, %d\n", v->r, v->g, v->b, v->a);
         break;

      case 0x14: // ST
         {
            float scale = (rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? 0.03125f : 0.015625f;
            v->ou = (float)((int16_t)(val>>16)) * scale;
            v->ov = (float)((int16_t)(val&0xFFFF)) * scale;
            v->uv_calculated = 0xFFFFFFFF;
            v->uv_scaled = 1;
         }
#if 0
         FRDP ("u/v: (%04lx, %04lx), (%f, %f)\n", (short)(val>>16), (short)(val&0xFFFF),
               v->ou, v->ov);
#endif
         break;

      case 0x18: // XY screen
         {
            float scr_x = (float)((int16_t)(val>>16)) / 4.0f;
            float scr_y = (float)((int16_t)(val&0xFFFF)) / 4.0f;
            v->screen_translated = 2;
            v->sx = scr_x * rdp.scale_x + rdp.offset_x;
            v->sy = scr_y * rdp.scale_y + rdp.offset_y;
            if (v->w < 0.01f)
            {
               v->w = 1.0f;
               v->oow = 1.0f;
               v->z_w = 1.0f;
            }
            v->sz = rdp.view_trans[2] + v->z_w * rdp.view_scale[2];

            v->scr_off = 0;
            if (scr_x < 0) v->scr_off |= 1;
            if (scr_x > rdp.vi_width) v->scr_off |= 2;
            if (scr_y < 0) v->scr_off |= 4;
            if (scr_y > rdp.vi_height) v->scr_off |= 8;
            if (v->w < 0.1f) v->scr_off |= 16;

            //FRDP ("x/y: (%f, %f)\n", scr_x, scr_y);
         }
         break;

      case 0x1C: // Z screen
         {
            float scr_z = (float)((short)(val>>16));
            v->z_w = (scr_z - rdp.view_trans[2]) / rdp.view_scale[2];
            v->z = v->z_w * v->w;
            //FRDP ("z: %f\n", scr_z);
         }
         break;

      default:
         //LRDP("UNKNOWN\n");
         break;
   }
}

//
// uc0:moveword - moves a word to someplace, like the segment pointers
//
static void uc0_moveword(uint32_t w0, uint32_t w1)
{
   // Find which command this is (lowest byte of cmd0)
   switch (_SHIFTR( w0, 0, 8))
   {
      case 0x00:
         RDP_E ("uc0:moveword matrix - IGNORED\n");
         LRDP("matrix - IGNORED\n");
         break;

      case G_MW_NUMLIGHT:
         rdp.num_lights = ((w1 - 0x80000000) >> 5) - 1; // inverse of equation
         if (rdp.num_lights > 8) rdp.num_lights = 0;

         rdp.update |= UPDATE_LIGHTS;
         //FRDP ("numlights: %d\n", rdp.num_lights);
         break;

      case G_MW_CLIP:
         if (((w0 >> 8)&0xFFFF) == 0x04)
         {
            rdp.clip_ratio = (float)vi_integer_sqrt(w1);
            rdp.update |= UPDATE_VIEWPORT;
         }
         break;

      case G_MW_SEGMENT:
         if ((w1 & BMASK)<BMASK)
            rdp.segment[(w0 >> 10) & 0x0F] = w1;
         break;
      case G_MW_FOG:
         rdp.fog_multiplier = (int16_t)(w1 >> 16);
         rdp.fog_offset = (int16_t)(w1 & 0x0000FFFF);
         break;
      case G_MW_LIGHTCOL:
         {
            int n = (w0 & 0xE000) >> 13;

            rdp.light[n].col[0] = (float)((w1 >> 24) & 0xFF) / 255.0f;
            rdp.light[n].col[1] = (float)((w1 >> 16) & 0xFF) / 255.0f;
            rdp.light[n].col[2] = (float)((w1 >> 8) & 0xFF) / 255.0f;
            rdp.light[n].col[3] = 255;
         }
         break;

      case 0x0c:
         {
            uint16_t val = (uint16_t)((w0 >> 8) & 0xFFFF);
            uint16_t vtx = val / 40;
            uint8_t where = val % 40;
            uc0_modifyvtx(where, vtx, w1);
            //FRDP ("uc0:modifyvtx: vtx: %d, where: 0x%02lx, val: %08lx - ", vtx, where, w1);
         }
         break;
#if 0
      case 0x0e:
         LRDP("perspnorm - IGNORED\n");
         break;
      default:
         FRDP_E ("uc0:moveword unknown (index: 0x%08lx)\n", rdp.cmd0 & 0xFF);
         FRDP ("unknown (index: 0x%08lx)\n", rdp.cmd0 & 0xFF);
#endif
   }
}


static void uc0_texture(uint32_t w0, uint32_t w1)
{
   int tile = (w0 >> 8) & 0x07;
   if (tile == 7 && (settings.hacks&hack_Supercross))
      tile = 0; //fix for supercross 2000
   rdp.mipmap_level = (w0 >> 11) & 0x07;
   rdp.cur_tile = tile;
   rdp.tiles[tile].on = 0;

   if ((w0 & 0xFF))
   {
      uint16_t s = (uint16_t)((w1 >> 16) & 0xFFFF);
      uint16_t t = (uint16_t)(w1 & 0xFFFF);

      rdp.tiles[tile].on = 1;
      rdp.tiles[tile].org_s_scale = s;
      rdp.tiles[tile].org_t_scale = t;
      rdp.tiles[tile].s_scale = (float)((s+1)/65536.0f) / 32.0f;
      rdp.tiles[tile].t_scale = (float)((t+1)/65536.0f) / 32.0f;

      rdp.update |= UPDATE_TEXTURE;
   }
}

static void uc0_setothermode_h(uint32_t w0, uint32_t w1)
{
   int i;
   int len   = _SHIFTR(w0, 0, 8);
   int shift = _SHIFTR(w0, 8, 8);
   uint32_t mask = 0;

   if ((settings.ucode == ucode_F3DEX2) || (settings.ucode == ucode_CBFD))
      shift = 32 - shift - (++len);

   i = len;
   for (; i; i--)
      mask = (mask << 1) | 1;
   mask <<= shift;

   rdp.cmd1 &= mask;
   rdp.othermode_h = (rdp.othermode_h & ~mask) | rdp.cmd1;

   if (mask & 0x00000030) // alpha dither mode
   {
      rdp.alpha_dither_mode = (rdp.othermode_h >> 4) & 0x3;
      FRDP ("alpha dither mode: %s\n", str_dither[rdp.alpha_dither_mode]);
   }

   if (mask & 0x00003000) // filter mode
   {
      rdp.filter_mode = (int)((rdp.othermode_h & 0x00003000) >> 12);
      rdp.update |= UPDATE_TEXTURE;
      FRDP ("filter mode: %s\n", str_filter[rdp.filter_mode]);
   }

   if (mask & 0x0000C000) // tlut mode
   {
      rdp.tlut_mode = (uint8_t)((rdp.othermode_h & 0x0000C000) >> 14);
      FRDP ("tlut mode: %s\n", str_tlut[rdp.tlut_mode]);
   }

   if (mask & 0x00300000) // cycle type
      rdp.update |= UPDATE_ZBUF_ENABLED;
}

static void uc0_setothermode_l(uint32_t w0, uint32_t w1)
{
   int i;
   int len   = _SHIFTR(w0, 0, 8);
   int shift = _SHIFTR(w0, 8, 8);
   uint32_t mask = 0;

   if ((settings.ucode == ucode_F3DEX2) || (settings.ucode == ucode_CBFD))
   {
      shift = 32 - shift - (++len);
      if (shift < 0) shift = 0;
   }

   i = len;
   for (; i; i--)
      mask = (mask << 1) | 1;
   mask <<= shift;

   rdp.cmd1 &= mask;
   rdp.othermode_l = (rdp.othermode_l & ~mask) | rdp.cmd1;

   if (mask & RDP_ALPHA_COMPARE) // alpha compare
      rdp.update |= UPDATE_ALPHA_COMPARE;

   if (mask & RDP_Z_SOURCE_SEL) // z-src selection
   {
      rdp.zsrc = (rdp.othermode_l & 0x00000004) >> 2;
      FRDP ("z-src sel: %s\n", str_zs[rdp.zsrc]);
      FRDP ("z-src sel: %08lx\n", rdp.zsrc);
      rdp.update |= UPDATE_ZBUF_ENABLED;
   }

   if (mask & 0xFFFFFFF8) // rendermode / blender bits
   {
      rdp.update |= UPDATE_FOG_ENABLED; //if blender has no fog bits, fog must be set off
      rdp.render_mode_changed |= rdp.rm ^ rdp.othermode_l;
      rdp.rm = rdp.othermode_l;
      if (settings.flame_corona && (rdp.rm == 0x00504341)) //hack for flame's corona
         rdp.othermode_l |= 0x00000010;
      FRDP ("rendermode: %08lx\n", rdp.othermode_l); // just output whole othermode_l
   }

   // there is not one setothermode_l that's not handled :)
}

static void uc0_setgeometrymode(uint32_t w0, uint32_t w1)
{
   rdp.geom_mode |= w1;
   //FRDP("uc0:setgeometrymode %08lx; result: %08lx\n", w1, rdp.geom_mode);

   if (w1 & 0x00000001) // Z-Buffer enable
   {
      if (!(rdp.flags & ZBUF_ENABLED))
      {
         rdp.flags |= ZBUF_ENABLED;
         rdp.update |= UPDATE_ZBUF_ENABLED;
      }
   }
   if (w1 & 0x00001000) // Front culling
   {
      if (!(rdp.flags & CULL_FRONT))
      {
         rdp.flags |= CULL_FRONT;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }
   if (w1 & 0x00002000) // Back culling
   {
      if (!(rdp.flags & CULL_BACK))
      {
         rdp.flags |= CULL_BACK;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }

   //Added by Gonetz
   if (w1 & 0x00010000) // Fog enable
   {
      if (!(rdp.flags & FOG_ENABLED))
      {
         rdp.flags |= FOG_ENABLED;
         rdp.update |= UPDATE_FOG_ENABLED;
      }
   }
}


static void uc0_cleargeometrymode(uint32_t w0, uint32_t w1)
{
   //FRDP("uc0:cleargeometrymode %08lx\n", w1);

   rdp.geom_mode &= ~w1;

   if (w1 & 0x00000001) // Z-Buffer enable
   {
      if (rdp.flags & ZBUF_ENABLED)
      {
         rdp.flags ^= ZBUF_ENABLED;
         rdp.update |= UPDATE_ZBUF_ENABLED;
      }
   }
   if (w1 & 0x00001000) // Front culling
   {
      if (rdp.flags & CULL_FRONT)
      {
         rdp.flags ^= CULL_FRONT;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }
   if (w1 & 0x00002000) // Back culling
   {
      if (rdp.flags & CULL_BACK)
      {
         rdp.flags ^= CULL_BACK;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }

   //Added by Gonetz
   if (w1 & 0x00010000) // Fog enable
   {
      if (rdp.flags & FOG_ENABLED)
      {
         rdp.flags ^= FOG_ENABLED;
         rdp.update |= UPDATE_FOG_ENABLED;
      }
   }
}

static void uc0_line3d(uint32_t w0, uint32_t w1)
{
   VERTEX *v[3];
   uint32_t v0 = ((w1 >> 16) & 0xff) / 10;
   uint32_t v1 = ((w1 >> 8) & 0xff) / 10;
   uint16_t width = (uint16_t)(w1 & 0xFF) + 3;
   uint32_t cull_mode = (rdp.flags & CULLMASK) >> CULLSHIFT;

   v[0] = &rdp.vtx[v1];
   v[1] = &rdp.vtx[v0];
   v[2] = &rdp.vtx[v0];

   rdp.flags |= CULLMASK;
   rdp.update |= UPDATE_CULL_MODE;
   cull_trianglefaces(v, 1, true, true, width);
   rdp.flags ^= CULLMASK;
   rdp.flags |= cull_mode << CULLSHIFT;
   rdp.update |= UPDATE_CULL_MODE;
}

static void uc0_tri4(uint32_t w0, uint32_t w1)
{
   // c0: 0000 0123, c1: 456789ab
   // becomes: 405 617 829 a3b

   VERTEX *v[12];

   if (rdp.skip_drawing)
      return;

   v[0]  = &rdp.vtx[(w1 >> 28) & 0xF]; /* v00 */
   v[1]  = &rdp.vtx[(w0 >> 12) & 0xF]; /* v01 */
   v[2]  = &rdp.vtx[(w1 >> 24) & 0xF]; /* v02 */
   v[3]  = &rdp.vtx[(w1 >> 20) & 0xF]; /* v10 */
   v[4]  = &rdp.vtx[(w0 >> 8)  & 0xF]; /* v11 */
   v[5]  = &rdp.vtx[(w1 >> 16) & 0xF]; /* v12 */
   v[6]  = &rdp.vtx[(w1 >> 12) & 0xF]; /* v20 */
   v[7]  = &rdp.vtx[(w0 >> 4) & 0xF];  /* v21 */
   v[8]  = &rdp.vtx[(w1 >> 8) & 0xF];  /* v22 */
   v[9]  = &rdp.vtx[(w1 >> 4) & 0xF];  /* v30 */
   v[10] = &rdp.vtx[(w0 >> 0) & 0xF];  /* v31 */
   v[11] = &rdp.vtx[(w1 >> 0) & 0xF];  /* v32 */

   cull_trianglefaces(v, 4, true, true, 0);
}
