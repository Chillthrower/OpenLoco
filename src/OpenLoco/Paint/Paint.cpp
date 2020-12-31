#include "Paint.h"
#include "../Interop/Interop.hpp"
#include "../Map/Tile.h"
#include "../StationManager.h"
#include "../TownManager.h"
#include "../Ui.h"
#include "../Ui/WindowManager.h"

using namespace OpenLoco::Interop;
using namespace OpenLoco::Ui::ViewportInteraction;

namespace OpenLoco::Paint
{
    static loco_global<InteractionItem, 0x00E40104> _sessionInteractionInfoType;
    static loco_global<uint8_t, 0x00E40105> _sessionInteractionInfoBh; // Unk var_29 of paintstruct
    static loco_global<int16_t, 0x00E40108> _sessionInteractionInfoX;
    static loco_global<int16_t, 0x00E4010A> _sessionInteractionInfoY;
    static loco_global<uint32_t, 0x00E4010C> _sessionInteractionInfoValue; // tileElement or thing ptr
    static loco_global<uint32_t, 0x00E40110> _getMapCoordinatesFromPosFlags;
    PaintSession _session;

    void PaintSession::init(Gfx::drawpixelinfo_t& dpi, const uint16_t viewportFlags)
    {
        _dpi = &dpi;
        _nextFreePaintStruct = &_paintEntries[0];
        _endOfPaintStructArray = &_paintEntries[3998];
        _lastPS = nullptr;
        for (auto& quadrant : _quadrants)
        {
            quadrant = nullptr;
        }
        _quadrantBackIndex = -1;
        _quadrantFrontIndex = 0;
        _lastPaintString = 0;
        _paintStringHead = 0;
    }

    // 0x0045A6CA
    PaintSession* allocateSession(Gfx::drawpixelinfo_t& dpi, uint16_t viewportFlags)
    {
        _session.init(dpi, viewportFlags);
        return &_session;
    }

    void registerHooks()
    {
        registerHook(
            0x004622A2,
            [](registers& regs) -> uint8_t {
                registers backup = regs;

                PaintSession session;
                session.generate();

                regs = backup;
                return 0;
            });
    }

    // 0x00461CF8
    static void paintTileElements(PaintSession& session, const Map::map_pos& loc)
    {
        registers regs{};
        regs.eax = loc.x;
        regs.ecx = loc.y;
        call(0x00461CF8, regs);
    }

    // 0x004617C6
    static void paintTileElements2(PaintSession& session, const Map::map_pos& loc)
    {
        registers regs{};
        regs.eax = loc.x;
        regs.ecx = loc.y;
        call(0x004617C6, regs);
    }

    // 0x0046FA88
    static void paintEntities(PaintSession& session, const Map::map_pos& loc)
    {
        registers regs{};
        regs.eax = loc.x;
        regs.ecx = loc.y;
        call(0x0046FA88, regs);
    }

    // 0x0046FB67
    static void paintEntities2(PaintSession& session, const Map::map_pos& loc)
    {
        registers regs{};
        regs.eax = loc.x;
        regs.ecx = loc.y;
        call(0x0046FB67, regs);
    }

    struct GenerationParameters
    {
        Map::map_pos mapLoc;
        uint16_t numVerticalQuadrants;
        std::array<Map::map_pos, 5> additionalQuadrants;
        Map::map_pos nextVerticalQuadrant;
    };

    template<uint8_t rotation>
    GenerationParameters generateParameters(Gfx::drawpixelinfo_t* context)
    {
        // TODO: Work out what these constants represent
        uint16_t numVerticalQuadrants = (context->height + (rotation == 0 ? 1040 : 1056)) >> 5;

        auto mapLoc = Ui::viewportCoordToMapCoord(static_cast<int16_t>(context->x & 0xFFE0), static_cast<int16_t>((context->y - 16) & 0xFFE0), 0, rotation);
        if constexpr (rotation & 1)
        {
            mapLoc.y -= 16;
        }
        mapLoc.x &= 0xFFE0;
        mapLoc.y &= 0xFFE0;

        constexpr uint8_t rotOrder[] = { 0, 3, 2, 1 };
        constexpr std::array<Map::map_pos, 5> additionalQuadrants = {
            Map::map_pos{ -32, 32 }.rotate(rotOrder[rotation]),
            Map::map_pos{ 0, 32 }.rotate(rotOrder[rotation]),
            Map::map_pos{ 32, 0 }.rotate(rotOrder[rotation]),
            Map::map_pos{ 32, -32 }.rotate(rotOrder[rotation]),
            Map::map_pos{ -32, 64 }.rotate(rotOrder[rotation]),
        };
        constexpr auto nextVerticalQuadrant = Map::map_pos{ 32, 32 }.rotate(rotOrder[rotation]);

        return { mapLoc, numVerticalQuadrants, additionalQuadrants, nextVerticalQuadrant };
    }

    void PaintSession::generateTilesAndEntities(GenerationParameters&& p)
    {
        for (; p.numVerticalQuadrants > 0; --p.numVerticalQuadrants)
        {
            paintTileElements(*this, p.mapLoc);
            paintEntities(*this, p.mapLoc);

            auto loc1 = p.mapLoc + p.additionalQuadrants[0];
            paintTileElements2(*this, loc1);
            paintEntities(*this, loc1);

            auto loc2 = p.mapLoc + p.additionalQuadrants[1];
            paintTileElements(*this, loc2);
            paintEntities(*this, loc2);

            auto loc3 = p.mapLoc + p.additionalQuadrants[2];
            paintTileElements2(*this, loc3);
            paintEntities(*this, loc3);

            auto loc4 = p.mapLoc + p.additionalQuadrants[3];
            paintEntities2(*this, loc4);

            auto loc5 = p.mapLoc + p.additionalQuadrants[4];
            paintEntities2(*this, loc5);

            p.mapLoc += p.nextVerticalQuadrant;
        }
    }

    // 0x004622A2
    void PaintSession::generate()
    {
        if ((addr<0x00525E28, uint32_t>() & (1 << 0)) == 0)
            return;

        currentRotation = Ui::WindowManager::getCurrentRotation();
        switch (currentRotation)
        {
            case 0:
                generateTilesAndEntities(generateParameters<0>(getContext()));
                break;
            case 1:
                generateTilesAndEntities(generateParameters<1>(getContext()));
                break;
            case 2:
                generateTilesAndEntities(generateParameters<2>(getContext()));
                break;
            case 3:
                generateTilesAndEntities(generateParameters<3>(getContext()));
                break;
        }
    }

    // 0x0045E7B5
    void PaintSession::arrangeStructs()
    {
        call(0x0045E7B5);
    }

    // 0x0045ED91
    [[nodiscard]] InteractionArg PaintSession::getNormalInteractionInfo(const uint32_t flags)
    {
        _sessionInteractionInfoType = InteractionItem::t_0;
        _getMapCoordinatesFromPosFlags = flags;
        call(0x0045ED91);
        return InteractionArg{ _sessionInteractionInfoX, _sessionInteractionInfoY, { _sessionInteractionInfoValue }, _sessionInteractionInfoType, _sessionInteractionInfoBh };
    }

    // 0x0048DDE4
    [[nodiscard]] InteractionArg PaintSession::getStationNameInteractionInfo(const uint32_t flags)
    {
        InteractionArg interaction{};

        // -2 as there are two interaction items that you can't filter out adjust in future
        if (flags & (1 << (static_cast<uint32_t>(InteractionItem::station) - 2)))
        {
            return interaction;
        }

        auto rect = (*_dpi)->getDrawableRect();

        for (auto& station : StationManager::stations())
        {
            if (station.empty())
            {
                continue;
            }

            if (station.flags & station_flags::flag_5)
            {
                continue;
            }

            if (!station.labelPosition.contains(rect, (*_dpi)->zoom_level))
            {
                continue;
            }

            interaction.type = InteractionItem::station;
            interaction.value = station.id();
        }

        // This is for functions that have not been implemented yet
        _sessionInteractionInfoType = interaction.type;
        _sessionInteractionInfoValue = interaction.value;
        return interaction;
    }

    // 0x0049773D
    [[nodiscard]] InteractionArg PaintSession::getTownNameInteractionInfo(const uint32_t flags)
    {
        InteractionArg interaction{};

        // -2 as there are two interaction items that you can't filter out adjust in future
        if (flags & (1 << (static_cast<uint32_t>(InteractionItem::town) - 2)))
        {
            return interaction;
        }

        auto rect = (*_dpi)->getDrawableRect();

        for (auto& town : TownManager::towns())
        {
            if (town.empty())
            {
                continue;
            }

            if (!town.labelPosition.contains(rect, (*_dpi)->zoom_level))
            {
                continue;
            }

            interaction.type = InteractionItem::town;
            interaction.value = town.id();
        }

        // This is for functions that have not been implemented yet
        _sessionInteractionInfoType = interaction.type;
        _sessionInteractionInfoValue = interaction.value;
        return interaction;
    }
}