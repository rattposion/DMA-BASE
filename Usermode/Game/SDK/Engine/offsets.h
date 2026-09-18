#pragma once

namespace SDK {
    namespace Offsets
    {
		constexpr auto timestamp = 0x6A3B107B;
		constexpr auto ref_def_ptr = 0xE7D57A8;
		constexpr auto name_array = 0xE95B900;
		constexpr auto name_array_pos = 0x3038;
		constexpr auto name_array_size = 0xD0;
		constexpr auto loot_ptr = 0xF965A60;
		constexpr auto loot_size = 0x850;
		constexpr auto loot_pos = 0x178;
		constexpr auto loot_valid1 = 0x83C;
		constexpr auto loot_valid2 = 0x848;
		constexpr auto camera_base = 0xE3D9850;
		constexpr auto camera_pos = 0x1F4;
		constexpr auto local_index = 0x1DA7E0;
		constexpr auto local_index_pos = 0x418;
		constexpr auto recoil = 0xFD11C;
		constexpr auto game_mode = 0xD9F6968;
		constexpr auto weapon_definitions = 0xE5BC320;
		constexpr auto distribute = 0xB029A90;
        constexpr uint32_t o_visible_bit = 0x9708C;
        constexpr uint32_t visible_client_bits = 0x9708C;
        constexpr uint32_t o_no_recoil = 0x1A1654;
        constexpr uint32_t Seed = 0xC4;
        constexpr uint32_t Angle = 0x648;
        constexpr uint32_t Player_client_state_enum = 0xD4A2C;
        constexpr uint32_t o_local_entity = 0x15BE30;
		constexpr auto scoreboard = 0xBB890;
		constexpr auto scoreboardsize = 0x80;
        constexpr uint32_t AimDownSightFrac = 0x1CAC3C;
        constexpr uint32_t BG_Ballistics_TravelTimeForDistance_DWord = 0x3F800000; // as float: 1
        constexpr uint32_t BG_GetBallisticInfo = 0x6029;
        constexpr uint32_t BG_GetBallisticInfo_Primary = 0x2BA8;
        constexpr uint32_t BG_GetBallisticInfo_Secondary = 0x2BD8;


        namespace Player
        {
			constexpr auto health = 0x600;
			constexpr auto size = 0x2768;
			constexpr auto valid = 0xBA2;
			constexpr auto pos = 0xDC0;
			constexpr auto team = 0x518;
			constexpr auto stance = 0x15FC;
			constexpr auto weapon_index = 0x2598;
			constexpr auto o_player_weapon_index = 0x180;
            constexpr auto dead_1 = 0x5B4;
            constexpr auto dead_2 = 0x139;
            constexpr auto dead_3 = 0x5B6;

        };

        namespace Bone
        {
			constexpr auto bone_base = 0x7D368;
			constexpr auto size = 0x1D0;
			constexpr auto offset = 0xE0;
        };

        namespace Entity
        {
			constexpr auto predictedPlayerEntity = 0x81D60;
			constexpr auto o_local_index = 0x570;
			constexpr auto o_entity_type = o_local_index + 0x8;
			constexpr auto o_local_valid = 0x70C;
			constexpr auto o_local_stance = 0x57C;
			constexpr auto o_local_pos = 0x594;
			constexpr auto o_local_size = 0x718;
        };
    }

    enum AXIS_VEC : int {
        FORWARD_VEC = 0,
        RIGHT_VEC = 1,
        UP_VEC = 2,
        MAX_AXIS_VEC,
    };

    struct RefDef_T {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
        Vec2    fov;
        char    pad1[8];
        char    pad2[4];
        Vec3Mem axis[MAX_AXIS_VEC];
    };

    class RefDef_Class {

    public:
        struct key {
            int32_t ref0;
            int32_t ref1;
            int32_t ref2;
        };

        auto GetRefDef() -> uintptr_t
        {
            key encrypted = DMAInterface::Read<key>(globals::g_baseAddress + Offsets::ref_def_ptr);

            DWORD lowerref = encrypted.ref0 ^ (encrypted.ref2 ^ (uint64_t)(globals::g_baseAddress + Offsets::ref_def_ptr)) * ((encrypted.ref2 ^ (uint64_t)(globals::g_baseAddress + Offsets::ref_def_ptr)) + 2);
            DWORD upperref = encrypted.ref1 ^ (encrypted.ref2 ^ (uint64_t)(globals::g_baseAddress + Offsets::ref_def_ptr + 0x4)) * ((encrypted.ref2 ^ (uint64_t)(globals::g_baseAddress + Offsets::ref_def_ptr + 0x4)) + 2); \
                return (uint64_t)upperref << 32 | lowerref;
        }
        RefDef_T ref_def_nn;
    }; inline RefDef_Class* DecryptRefDef = new RefDef_Class();



    
   










    inline uintptr_t decrypt_client_info()
    {
        const uint64_t mb = globals::g_baseAddress;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rbx = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xE7C2688);
        if (!rbx)
            return rbx;
        rdx = ~globals::g_peb;
        rax = rbx;
        rax >>= 0x18;
        rcx = 0;
        rax ^= rbx;
        rcx = _rotl64(rcx, 0x10);
        rcx ^= DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC19);
        rbx = rax;
        rbx >>= 0x30;
        rcx = ~rcx;
        rbx ^= rax;
        rax = 0x233F5F4AE79533B1;
        rbx *= rax;
        rax = 0x4FF2ED27F19D575D;
        rbx -= rdx;
        rbx += rax;
        rbx ^= r9;
        rbx *= DMAInterface::Read<uintptr_t>(rcx + 0x19);
        return rbx;
    }
    inline uintptr_t decrypt_client_base(uintptr_t client_info)
    {
        const uint64_t mb = globals::g_baseAddress;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        r8 = DMAInterface::Read<uintptr_t>(client_info + 0x22f7c8);
        if (!r8)
            return r8;
        rbx = globals::g_peb;
        rax = rbx;
        rax <<= 0x23;
        rax = _byteswap_uint64(rax);
        rax &= 0xF;
        switch (rax) {
        case 0:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            rax = globals::g_baseAddress;
            r8 -= rax;
            rax = r8;
            rax >>= 0x1E;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x3C;
            r8 ^= rax;
            rax = globals::g_baseAddress;
            r8 -= rax;
            rax = r8;
            rax >>= 0x28;
            r8 ^= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = 0x9CC8E0420ADA280D;
            rax *= r8;
            rax += rbx;
            r8 = rax;
            r8 >>= 0x11;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x22;
            r8 ^= rax;
            return r8;
        }
        case 1:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r15 = globals::g_baseAddress + 0x755F7BDD;
            rax = r8;
            rax >>= 0x9;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x12;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x24;
            r8 ^= rax;
            r8 ^= rbx;
            rcx = 0;
            rcx = _rotl64(rcx, 0x10);
            rcx ^= r10;
            rcx = ~rcx;
            r8 *= DMAInterface::Read<uintptr_t>(rcx + 0x9);
            rcx = globals::g_baseAddress;
            rax = rbx;
            rax -= rcx;
            rax += 0xFFFFFFFF9F0CFAED;
            r8 += rax;
            rax = 0x40ED86BABDEA8F5B;
            r8 *= rax;
            rax = 0xA7798517B7F399EA;
            r8 ^= rax;
            rax = r15;
            rax = ~rax;
            rax ^= rbx;
            r8 += rax;
            rax = 0x459093E765583ADB;
            r8 *= rax;
            return r8;
        }
        case 2:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r14 = globals::g_baseAddress + 0xAC81;
            rax = 0xE03443781C6DB26D;
            r8 *= rax;
            rax = 0x26676A6627BAC50C;
            r8 -= rax;
            rax = 0x541ECC7788F37ADE;
            r8 += rax;
            r8 += r14;
            rax = globals::g_baseAddress + 0x142;
            rax = ~rax;
            rcx = rbx;
            rcx = ~rcx;
            rcx -= rbx;
            rcx += rax;
            r8 += rcx;
            rax = r8;
            rax >>= 0x15;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x2A;
            r8 ^= rax;
            rax = globals::g_baseAddress;
            r8 -= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            return r8;
        }
        case 3:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r13 = globals::g_baseAddress + 0xF26D;
            r8 ^= rbx;
            rcx = 0;
            rcx = _rotl64(rcx, 0x10);
            rcx ^= r10;
            rcx = ~rcx;
            r8 *= DMAInterface::Read<uintptr_t>(rcx + 0x9);
            rax = 0x6C5618A3BE4C414;
            r8 -= rax;
            rax = 0xE98709096AD185CC;
            r8 ^= rax;
            rax = r8;
            rax >>= 0xB;
            rax ^= r8;
            r8 = globals::g_baseAddress + 0x5ED318FB;
            rcx = rax;
            r8 = ~r8;
            r8 *= rbx;
            rcx >>= 0x16;
            rcx ^= rax;
            rax = rcx;
            rax >>= 0x2C;
            r8 ^= rax;
            r8 ^= rcx;
            rax = 0x22A1571E2E749CB;
            r8 *= rax;
            rax = rbx;
            rax *= r13;
            r8 += rax;
            return r8;
        }
        case 4:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r15 = globals::g_baseAddress + 0x72D0A311;
            rax = 0x54EE9012A77B3C0E;
            r8 ^= rax;
            rax = globals::g_baseAddress;
            rax += 0x432D;
            rax += rbx;
            r8 += rax;
            rax = 0xBE6A84FFF3304C3D;
            r8 *= rax;
            rax = r8;
            r8 >>= 0x12;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x24;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x7;
            r8 ^= rax;
            rax = r8;
            rax >>= 0xE;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x1C;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x38;
            r8 ^= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = rbx;
            rax *= r15;
            r8 -= rax;
            rax = 0x598660DAA37ACC99;
            r8 ^= rax;
            return r8;
        }
        case 5:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = 0xC088FB236BE68165;
            r8 *= rax;
            rax = r8;
            r8 >>= 0x5;
            r8 ^= rax;
            rax = r8;
            rax >>= 0xA;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x14;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x28;
            r8 ^= rax;
            rax = r8;
            rax >>= 0xB;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x16;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x2C;
            r8 ^= rax;
            rax = 0xF87FD44152069748;
            r8 ^= rax;
            rax = globals::g_baseAddress;
            rax += 0x1079;
            rax += rbx;
            r8 ^= rax;
            rcx = globals::g_baseAddress;
            rax = rbx;
            rax = ~rax;
            rax -= rcx;
            rax += 0xFFFFFFFF968271AB;
            r8 += rax;
            return r8;
        }
        case 6:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r15 = globals::g_baseAddress + 0x1EE2;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rcx = 0x30DABF93D6E4FB5;
            r8 ^= rcx;
            rax = rbx;
            rax ^= r15;
            r8 -= rax;
            rax = 0xDB8B0AAFA542904;
            r8 -= rbx;
            r8 -= rax;
            rax = r8;
            rax >>= 0x22;
            r8 ^= rax;
            rax = 0xDF170407BBE28DB5;
            r8 *= rax;
            rax = r8;
            r8 >>= 0x8;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x10;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x20;
            r8 ^= rax;
            return r8;
        }
        case 7:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r15 = globals::g_baseAddress + 0xC177;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = 0x1C4A7DE2E2F8F68F;
            rcx = 0x378CE09B287B2D41;
            rcx ^= r8;
            rcx += rax;
            r8 = rcx;
            r8 >>= 0x23;
            r8 ^= rcx;
            rax = rbx + 0x1;
            rax *= r15;
            rax += rbx;
            r8 += rax;
            rax = 0xEBEA9B8B5714671D;
            r8 *= rax;
            rax = r8;
            rax >>= 0xE;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x1C;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x38;
            r8 ^= rax;
            return r8;
        }
        case 8:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r13 = globals::g_baseAddress + 0x553;
            r15 = globals::g_baseAddress + 0x88B9;
            rax = globals::g_baseAddress;
            r8 ^= rax;
            rax = 0x3169FBDB3B875224;
            r8 += rax;
            rax = r15;
            rax = ~rax;
            rax *= rbx;
            r8 ^= rax;
            r8 ^= rbx;
            r8 ^= r13;
            rax = r8;
            rax >>= 0x13;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x26;
            r8 ^= rax;
            rcx = 0;
            rcx = _rotl64(rcx, 0x10);
            rax = 0x49665D7F2AFA3F6B;
            r8 *= rax;
            rcx ^= r10;
            rax = globals::g_baseAddress + 0x11D125F7;
            rax = ~rax;
            rcx = ~rcx;
            rax *= rbx;
            r8 += rax;
            r8 *= DMAInterface::Read<uintptr_t>(rcx + 0x9);
            return r8;
        }
        case 9:
        {
            r9 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r11 = globals::g_baseAddress + 0x7C81;
            rax = rbx;
            rax *= r11;
            r8 -= rax;
            rax = globals::g_baseAddress;
            r8 -= rax;
            rax = rbx;
            rax -= globals::g_baseAddress;
            rax += 0xFFFFFFFFFFFF4D38;
            r8 += rax;
            rax = 0xB294869EA09D48AA;
            r8 ^= rax;
            rax = 0xDA6A9700AB4D27FD;
            r8 *= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r9;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = 0x38632CDC13FD78A5;
            r8 += rax;
            rax = r8;
            rax >>= 0x1D;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x3A;
            r8 ^= rax;
            return r8;
        }
        case 10:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r8 += rbx;
            rax = 0x36164EFD786890C1;
            r8 *= rax;
            rax = 0x6F993F33D7A49418;
            rax += r8;
            r8 = rax;
            r8 >>= 0x8;
            r8 ^= rax;
            rcx = r8;
            rcx >>= 0x10;
            rcx ^= r8;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            r8 = rcx;
            rax ^= r10;
            r8 >>= 0x20;
            r8 ^= rcx;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = 0xE88B55E25B8B057C;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x1A;
            rax ^= r8;
            rcx = rax;
            rcx >>= 0x34;
            rcx ^= rax;
            rax = rcx;
            rax >>= 0x4;
            rax ^= rcx;
            rcx = rax;
            rcx >>= 0x8;
            rcx ^= rax;
            r8 = rcx;
            r8 >>= 0x10;
            r8 ^= rcx;
            rax = r8;
            rax >>= 0x20;
            r8 ^= rax;
            return r8;
        }
        case 11:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = rbx;
            uintptr_t RSP_0xFFFFFFFFFFFFFFB8;
            RSP_0xFFFFFFFFFFFFFFB8 = globals::g_baseAddress + 0xA1FD;
            rax *= RSP_0xFFFFFFFFFFFFFFB8;
            r8 += rax;
            rax = r8;
            rax >>= 0x26;
            r8 ^= rax;
            rax = r8;
            rax >>= 0xA;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x14;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x28;
            r8 ^= rax;
            rax = 0xC6A8E21F37CF3675;
            r8 *= rax;
            rax = globals::g_baseAddress;
            rax += rbx;
            r8 -= rax;
            rax = globals::g_baseAddress;
            r8 ^= rax;
            return r8;
        }
        case 12:
        {
            r9 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            rax = 0x5D2901AC55739352;
            r8 -= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r9;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            rax = globals::g_baseAddress;
            r8 += rax;
            rax = 0x156D71AB28FBFAFF;
            r8 *= rax;
            rax = r8;
            r8 >>= 0x27;
            r8 ^= rax;
            r8 -= rbx;
            rax = r8;
            rax >>= 0x17;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x2E;
            r8 ^= rax;
            rax = globals::g_baseAddress;
            r8 ^= rax;
            return r8;
        }
        case 13:
        {
            r11 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            rax = r8;
            rax >>= 0x1F;
            r8 ^= rax;
            rcx = r8;
            rax = globals::g_baseAddress;
            rcx >>= 0x3E;
            rcx ^= r8;
            rdx = 0;
            rdx = _rotl64(rdx, 0x10);
            r8 = rbx;
            r8 = ~r8;
            rdx ^= r11;
            r8 += rcx;
            rdx = ~rdx;
            r8 -= rax;
            r8 -= 0x6929AFAC;
            r8 *= DMAInterface::Read<uintptr_t>(rdx + 0x9);
            rax = r8;
            rax >>= 0x18;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x30;
            r8 ^= rax;
            rax = 0x69651B1AF033619B;
            r8 += rbx;
            r8 *= rax;
            rax = 0x29BBD1B30DFD9417;
            r8 *= rax;
            rax = 0xA7B8F15C4FABBB6C;
            r8 ^= rax;
            return r8;
        }
        case 14:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r8 += rbx;
            rax = globals::g_baseAddress + 0x8D0;
            rax -= rbx;
            r8 += rax;
            rax = 0xBC0AAA7E98B1663A;
            r8 ^= rax;
            rax = 0x54D1F9305B205B45;
            r8 *= rax;
            rcx = r8;
            rcx >>= 0xA;
            rcx ^= r8;
            rax = rcx;
            rax >>= 0x14;
            rax ^= rcx;
            r8 = rax;
            r8 >>= 0x28;
            r8 ^= rax;
            rax = r8;
            rax >>= 0x12;
            rax ^= r8;
            r8 = rax;
            r8 >>= 0x24;
            r8 ^= rax;
            rax = 0xFFFFFFFFDE23E20A;
            rax -= rbx;
            rax -= globals::g_baseAddress;
            r8 += rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            r8 *= DMAInterface::Read<uintptr_t>(rax + 0x9);
            return r8;
        }
        case 15:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CEC4A);
            r15 = globals::g_baseAddress + 0x76BB;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rax = DMAInterface::Read<uintptr_t>(rax + 0x9);
            uintptr_t RSP_0xFFFFFFFFFFFFFFB0;
            RSP_0xFFFFFFFFFFFFFFB0 = 0x3A27415DA31CA989;
            rax *= RSP_0xFFFFFFFFFFFFFFB0;
            r8 *= rax;
            rax = 0x6F6A3BE0CADE4A54;
            r8 -= rax;
            r8 -= rbx;
            rcx = r8;
            rcx >>= 0x13;
            rcx ^= r8;
            r8 = rbx;
            r8 = ~r8;
            rax = r15;
            rax = ~rax;
            r8 *= rax;
            rax = rcx;
            rax >>= 0x26;
            rax ^= rcx;
            r8 += rax;
            rax = r8;
            rax >>= 0x28;
            r8 ^= rax;
            rax = 0x3224CE0A9BEB6A6E;
            r8 -= rax;
            return r8;
        }
        }
    }

    inline uintptr_t decrypt_bone_base()
    {
        const uint64_t mb = globals::g_baseAddress;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rdx = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0x1350D078);
        if (!rdx)
            return rdx;
        r11 = globals::g_peb;
        rax = r11;
        rax >>= 0x13;
        rax &= 0xF;
        switch (rax) {
        case 0:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            rax = globals::g_baseAddress + 0x8C93;
            rax -= r11;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x15;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x2A;
            rdx ^= rax;
            rdx += r11;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = globals::g_baseAddress;
            rdx ^= rax;
            rax = 0x860534C8C01FEA7B;
            rdx *= rax;
            rax = 0xEE334BF3EC572D68;
            rdx ^= rax;
            return rdx;
        }
        case 1:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0xDF5D;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = 0x8A4B98169395E686;
            rdx ^= rax;
            rax = 0xC3957EB9F84EC5AF;
            rdx *= rax;
            rax = rdx;
            rax >>= 0xE;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x1C;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x38;
            rdx ^= rax;
            rax = globals::g_baseAddress + 0x31CB;
            rax -= r11;
            rdx += rax;
            rax = rdx;
            rax >>= 0xD;
            rax ^= rdx;
            rcx = rax;
            rcx >>= 0x1A;
            rcx ^= rax;
            rdx = rcx;
            rdx >>= 0x34;
            rdx ^= rcx;
            rax = r15;
            rax = ~rax;
            rdx ^= rax;
            rdx ^= r11;
            return rdx;
        }
        case 2:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r12 = globals::g_baseAddress + 0x47C2AE1B;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = r12;
            rax = ~rax;
            rax ^= r11;
            rdx += rax;
            rax = 0x94073D91C803188D;
            rdx += r11;
            rdx ^= rax;
            rax = 0x2EEA8A0831CE333B;
            rdx *= rax;
            rdx += r11;
            rax = rdx;
            rax >>= 0x13;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x26;
            rdx ^= rax;
            rax = 0xD4E2CCE5B7959CA0;
            rdx ^= rax;
            return rdx;
        }
        case 3:
        {
            r9 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r12 = globals::g_baseAddress + 0x114B;
            rax = rdx;
            rax >>= 0x13;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x26;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x24;
            rdx ^= rax;
            rax = 0x764F15DD269101D3;
            rdx *= rax;
            rax = 0x34E81942B113C230;
            rdx -= rax;
            rax = 0x13805FC46F4FC36A;
            rdx += rax;
            rax = r11;
            rax -= globals::g_baseAddress;
            rax += 0xFFFFFFFFFFFF85F3;
            rdx += rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r9;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rdx ^= r12;
            rdx ^= r11;
            return rdx;
        }
        case 4:
        {
            r9 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            rax = rdx;
            rax >>= 0x11;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x22;
            rdx ^= rax;
            rax = 0x2CFB6FB2F3BAD3C;
            rdx -= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r9;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = 0xEED0F28134CE8447;
            rdx *= rax;
            rax = 0x52D4170A67BFFCB2;
            rdx ^= rax;
            rax = rdx + r11 * 1;
            rdx = rax;
            rdx >>= 0x16;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x2C;
            rdx ^= rax;
            rdx ^= r11;
            return rdx;
        }
        case 5:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0x19B7DBCB;
            r12 = globals::g_baseAddress + 0x654BDD13;
            rax = r12;
            rax = ~rax;
            rax++;
            rdx += rax;
            rax = 0x4A2AFA53025C5181;
            rdx += r11;
            rdx *= rax;
            rax = rdx;
            rax >>= 0x28;
            rdx ^= rax;
            rax = r11 + r15 * 1;
            rcx = globals::g_baseAddress + 0xA045;
            rcx += r11;
            rcx ^= rax;
            rdx ^= rcx;
            rax = 0x574A3A5B7408079B;
            rdx *= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            return rdx;
        }
        case 6:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0x4951;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rdx += r11;
            rax = r15;
            rax *= r11;
            rdx += rax;
            rcx = globals::g_baseAddress;
            rcx += 0x1D37B933;
            rcx += rdx;
            rcx += r11;
            rdx = rcx;
            rdx >>= 0x9;
            rdx ^= rcx;
            rax = rdx;
            rax >>= 0x12;
            rax ^= rdx;
            rcx = rax;
            rcx >>= 0x24;
            rcx ^= rax;
            rax = 0x6C2A29044A40E4C7;
            rcx *= rax;
            rax = globals::g_baseAddress;
            rcx ^= rax;
            rax = rcx;
            rax >>= 0x3;
            rax ^= rcx;
            rcx = rax;
            rcx >>= 0x6;
            rcx ^= rax;
            rax = rcx;
            rax >>= 0xC;
            rax ^= rcx;
            rdx = rax;
            rdx >>= 0x18;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x30;
            rdx ^= rax;
            return rdx;
        }
        case 7:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0xCEFB;
            rax = globals::g_baseAddress;
            rdx += rax;
            rax = 0x5F80490A38DB3901;
            rdx ^= rax;
            rax = 0x4EC9DC6A5902297D;
            rdx -= rax;
            rax = rdx;
            rax >>= 0x25;
            rdx ^= rax;
            rax = r15;
            rax ^= r11;
            rdx += rax;
            rax = 0x92B34BC27C367071;
            rdx *= rax;
            rdx -= r11;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            return rdx;
        }
        case 8:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r13 = globals::g_baseAddress + 0x5723;
            r12 = globals::g_baseAddress + 0xFAB2;
            rax = 0xE62DA6375F493113;
            rdx *= rax;
            rax = globals::g_baseAddress;
            rdx -= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rdx -= r11;
            rax = rdx;
            rax >>= 0xF;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x1E;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x3C;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x3;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x6;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0xC;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x18;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x30;
            rdx ^= rax;
            rax = r11 + r12 * 1;
            rdx ^= rax;
            rdx ^= r13;
            rdx ^= r11;
            return rdx;
        }
        case 9:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r12 = globals::g_baseAddress + 0xF1EC;
            r13 = globals::g_baseAddress + 0x5304B0E6;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = r11 + r12 * 1;
            rcx = globals::g_baseAddress;
            rcx += 0x429D;
            rcx += rdx;
            rcx += r11;
            rcx ^= rax;
            rcx ^= r13;
            rcx ^= r11;
            rdx = rcx;
            rdx >>= 0x22;
            rdx ^= rcx;
            rax = 0xEE899EDDAF56550;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0xE;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x1C;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x38;
            rdx ^= rax;
            rax = 0x39D515C223A57391;
            rdx *= rax;
            return rdx;
        }
        case 10:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r12 = globals::g_baseAddress + 0x1A3D;
            rax = rdx;
            rax >>= 0xF;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x1E;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x3C;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x13;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x26;
            rdx ^= rax;
            rax = r11;
            rax = ~rax;
            rax *= r12;
            rdx ^= rax;
            rax = 0x8330B389343DA675;
            rdx *= rax;
            rax = 0x5A325A7184C15E55;
            rdx -= rax;
            rax = 0xE28957C95B7E497;
            rdx += rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rdx -= r11;
            return rdx;
        }
        case 11:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r14 = globals::g_baseAddress + 0x67B591A2;
            rax = rdx;
            rax >>= 0x22;
            rdx ^= rax;
            rax = r14;
            rax = ~rax;
            rax ^= r11;
            rax += r11;
            rdx -= rax;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rdx ^= r11;
            rax = 0x112AEF7CBA9BEDF1;
            rdx *= rax;
            rax = 0x792205E77EAA6797;
            rdx ^= rax;
            return rdx;
        }
        case 12:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0x70E4B3E1;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = 0x33BF00DD8A073650;
            rdx -= rax;
            rax = rdx;
            rax >>= 0xA;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x14;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x28;
            rdx ^= rax;
            rax = globals::g_baseAddress;
            rdx ^= rax;
            rax = r15;
            rax = ~rax;
            rdx += rax;
            rax = 0x37300D9E69A77B2F;
            rdx *= rax;
            rdx -= r11;
            return rdx;
        }
        case 13:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r14 = globals::g_baseAddress + 0x666C9DA0;
            rax = r14;
            rax ^= r11;
            rdx -= rax;
            rax = 0x124569EA4125D98;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x5;
            rax ^= rdx;
            rcx = rax;
            rcx >>= 0xA;
            rcx ^= rax;
            rdx = rcx;
            rdx >>= 0x14;
            rdx ^= rcx;
            rax = rdx;
            rax >>= 0x28;
            rax ^= rdx;
            rdx = rax;
            rdx >>= 0x1A;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x34;
            rcx = 0;
            rdx ^= rax;
            rcx = _rotl64(rcx, 0x10);
            rcx ^= r10;
            rcx = ~rcx;
            rdx *= DMAInterface::Read<uintptr_t>(rcx + 0x17);
            rdx ^= r11;
            rax = 0xD83F30F92C64DF4F;
            rdx ^= rax;
            rax = 0xB69AFD2628432A9D;
            rdx *= rax;
            return rdx;
        }
        case 14:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r15 = globals::g_baseAddress + 0x5113;
            rax = rdx;
            rax >>= 0x1B;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x36;
            rcx = 0;
            rdx ^= rax;
            rcx = _rotl64(rcx, 0x10);
            rcx ^= r10;
            rcx = ~rcx;
            rdx *= DMAInterface::Read<uintptr_t>(rcx + 0x17);
            rax = 0xDC4274449EFE767B;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x6;
            rax ^= rdx;
            rcx = rax;
            rcx >>= 0xC;
            rcx ^= rax;
            rdx = rcx;
            rdx >>= 0x18;
            rdx ^= rcx;
            rax = rdx;
            rax >>= 0x30;
            rdx ^= rax;
            rax = r15;
            rax ^= r11;
            rdx -= rax;
            rax = 0x4480AA60A21867F9;
            rdx *= rax;
            rax = globals::g_baseAddress + 0xD03A;
            rax += r11;
            rdx += rax;
            return rdx;
        }
        case 15:
        {
            r10 = DMAInterface::Read<uintptr_t>(globals::g_baseAddress + 0xD5CED3D);
            r13 = globals::g_baseAddress + 0x642A39AC;
            r12 = globals::g_baseAddress + 0x6744783A;
            rdx += r11;
            rax = r11;
            rax = ~rax;
            rax ^= r13;
            rdx -= rax;
            rdx ^= r12;
            rdx ^= r11;
            rax = 0;
            rax = _rotl64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= DMAInterface::Read<uintptr_t>(rax + 0x17);
            rax = 0x54750E0E4638841A;
            rdx += rax;
            rax = 0x17257FE07A931EB4;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x4;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x8;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x10;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x20;
            rdx ^= rax;
            rax = 0x7493CCED6314B08B;
            rdx *= rax;
            return rdx;
        }
        }
    }

    inline uint16_t get_bone_index(uint32_t bone_index)
    {
        const uint64_t mb = globals::g_baseAddress;
        uint64_t rax = mb, rbx = mb, rcx = mb, rdx = mb, rdi = mb, rsi = mb, r8 = mb, r9 = mb, r10 = mb, r11 = mb, r12 = mb, r13 = mb, r14 = mb, r15 = mb;
        rbx = bone_index;
        rcx = rbx * 0x13C8;
        rax = 0xCB182C584BD5193;
        r11 = globals::g_baseAddress;
        rax = _umul128(rax, rcx, (uintptr_t*)&rdx); 		//mul rcx
        rax = rcx; 		//mov rax, rcx
        r10 = 0xD6FB75C08B670E5B; 		//mov r10, 0xD6FB75C08B670E5B
        rax -= rdx; 		//sub rax, rdx
        rax >>= 0x1; 		//shr rax, 0x01
        rax += rdx; 		//add rax, rdx
        rax >>= 0xC; 		//shr rax, 0x0C
        rax = rax * 0x1E7D; 		//imul rax, rax, 0x1E7D
        rcx -= rax; 		//sub rcx, rax
        rax = 0x4078E2A8FCDA18EF; 		//mov rax, 0x4078E2A8FCDA18EF
        r8 = rcx * 0x1E7D; 		//imul r8, rcx, 0x1E7D
        rax = _umul128(rax, r8, (uintptr_t*)&rdx); 		//mul r8
        rdx >>= 0xB; 		//shr rdx, 0x0B
        rax = rdx * 0x1FC4; 		//imul rax, rdx, 0x1FC4
        r8 -= rax; 		//sub r8, rax
        rax = 0xF0F0F0F0F0F0F0F1; 		//mov rax, 0xF0F0F0F0F0F0F0F1
        rax = _umul128(rax, r8, (uintptr_t*)&rdx); 		//mul r8
        rax = 0x624DD2F1A9FBE77; 		//mov rax, 0x624DD2F1A9FBE77
        rdx >>= 0x6; 		//shr rdx, 0x06
        rcx = rdx * 0x44; 		//imul rcx, rdx, 0x44
        rax = _umul128(rax, r8, (uintptr_t*)&rdx); 		//mul r8
        rax = r8; 		//mov rax, r8
        rax -= rdx; 		//sub rax, rdx
        rax >>= 0x1; 		//shr rax, 0x01
        rax += rdx; 		//add rax, rdx
        rax >>= 0x6; 		//shr rax, 0x06
        rcx += rax; 		//add rcx, rax
        rax = rcx * 0xFA; 		//imul rax, rcx, 0xFA
        rcx = r8 * 0xFC; 		//imul rcx, r8, 0xFC
        rcx -= rax; 		//sub rcx, rax
        rax = DMAInterface::Read<uint16_t>(rcx + r11 * 1 + 0xC1E5230);
        r8 = rax * 0x13C8; 		//imul r8, rax, 0x13C8
        rax = r10; 		//mov rax, r10
        rax = _umul128(rax, r8, (uintptr_t*)&rdx); 		//mul r8
        rax = r10; 		//mov rax, r10
        rdx >>= 0xD; 		//shr rdx, 0x0D
        rcx = rdx * 0x261B; 		//imul rcx, rdx, 0x261B
        r8 -= rcx; 		//sub r8, rcx
        r9 = r8 * 0x2F75; 		//imul r9, r8, 0x2F75
        rax = _umul128(rax, r9, (uintptr_t*)&rdx); 		//mul r9
        rdx >>= 0xD; 		//shr rdx, 0x0D
        rax = rdx * 0x261B; 		//imul rax, rdx, 0x261B
        r9 -= rax; 		//sub r9, rax
        rax = 0x8FB823EE08FB823F; 		//mov rax, 0x8FB823EE08FB823F
        rax = _umul128(rax, r9, (uintptr_t*)&rdx); 		//mul r9
        rax = 0x579D6EE340579D6F; 		//mov rax, 0x579D6EE340579D6F
        rdx >>= 0x5; 		//shr rdx, 0x05
        rcx = rdx * 0x39; 		//imul rcx, rdx, 0x39
        rax = _umul128(rax, r9, (uintptr_t*)&rdx); 		//mul r9
        rdx >>= 0x6; 		//shr rdx, 0x06
        rcx += rdx; 		//add rcx, rdx
        rax = rcx * 0x176; 		//imul rax, rcx, 0x176
        rcx = r9 * 0x178; 		//imul rcx, r9, 0x178
        rcx -= rax; 		//sub rcx, rax
        r14 = DMAInterface::Read<uint16_t>(rcx + r11 * 1 + 0xC1E94A0);
        return r14;
    }
}
