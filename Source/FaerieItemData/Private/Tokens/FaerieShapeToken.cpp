// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "Tokens/FaerieShapeToken.h"

#include "FaerieInventoryContent/Public/Extensions/InventorySpatialGridExtension.h"

struct FSpatialKeyedEntry;

bool UFaerieShapeToken::FitsInGrid(const FIntPoint& GridSize, const FIntPoint& Position,
                                   const FSpatialContent& Occupied) const
{
    for (const FIntPoint& Coord : ShapeCoords)
    {
        FIntPoint AbsolutePosition = Position + Coord;

        if (AbsolutePosition.X < 0 || AbsolutePosition.X >= GridSize.X ||
            AbsolutePosition.Y < 0 || AbsolutePosition.Y >= GridSize.Y)
        {
            return false;
        }

        for (const FSpatialKeyedEntry& Entry : Occupied.GetEntries())
        {
            if (Entry.Key.Key == AbsolutePosition)
            {
                return false;
            }
        }
    }
    return true;
}

TArray<FIntPoint> UFaerieShapeToken::GetOccupiedPositions(const FIntPoint& Position) const
{
    TArray<FIntPoint> OccupiedPositions;
    for (const FIntPoint& Coord : ShapeCoords)
    {
        OccupiedPositions.Add(Position + Coord);
    }
    return OccupiedPositions;
}

FIntPoint UFaerieShapeToken::GetWhereCanFit(const FIntPoint& GridSize, const FSpatialContent& Occupied) const
{
    for (int32 Y = 0; Y < GridSize.Y; Y++)
    {
        for (int32 X = 0; X < GridSize.X; X++)
        {
            FIntPoint TestPosition(X, Y);
            if (FitsInGrid(GridSize, TestPosition, Occupied))
            {
                return TestPosition;
            }
        }
    }
    return FIntPoint(-1, -1);
}
