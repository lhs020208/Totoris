#include "TotorisMinoRichTextDecorator.h"

#include "Components/RichTextBlock.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	class STotorisInlineMino final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STotorisInlineMino) {}
			SLATE_ARGUMENT(TCHAR, Type)
			SLATE_ARGUMENT(float, IconHeight)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Type = InArgs._Type;
			IconHeight = InArgs._IconHeight;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			const int32 Columns = Type == TEXT('I') ? 4 : (Type == TEXT('O') ? 2 : 3);
			return FVector2D(IconHeight * Columns * .5f, IconHeight);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
			const FSlateRect& CullingRect, FSlateWindowElementList& DrawElements,
			int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
		{
			const TArray<FIntPoint>& Cells = GetCells(Type);
			const int32 Columns = Type == TEXT('I') ? 4 : (Type == TEXT('O') ? 2 : 3);
			const float CellSize = Geometry.GetLocalSize().Y * .5f;
			const float ShapeWidth = Columns * CellSize;
			const float OffsetX = (Geometry.GetLocalSize().X - ShapeWidth) * .5f;
			// Mission glyphs sit one mino cell higher than their original baseline.
			// I is deliberately lowered by the same amount, preserving its existing
			// mission-panel position while the other six types move up.
			const float MissionRaise = CellSize;
			const float ICompensation = Type == TEXT('I') ? CellSize : 0.f;
			const float OffsetY = (Geometry.GetLocalSize().Y - CellSize * 2.f) * .5f
				- MissionRaise + ICompensation;
			// Keep the tetromino's own identity color. Unlike the surrounding
			// mission text, tier and inactive-slot colors must not tint this glyph.
			const FLinearColor Fill = GetBaseColor(Type) * WidgetStyle.GetColorAndOpacityTint();
			const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			for (const FIntPoint& Cell : Cells)
			{
				const FVector2D Position(OffsetX + Cell.X * CellSize, OffsetY + Cell.Y * CellSize);
				FSlateDrawElement::MakeBox(DrawElements, LayerId, Geometry.ToPaintGeometry(
					FVector2D(CellSize - 1.f, CellSize - 1.f), FSlateLayoutTransform(Position)),
					Brush, ESlateDrawEffect::None, Fill);
			}
			return LayerId;
		}

	private:
		static const TArray<FIntPoint>& GetCells(TCHAR InType)
		{
			static const TArray<FIntPoint> I = { {0, 0}, {1, 0}, {2, 0}, {3, 0} };
			static const TArray<FIntPoint> O = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
			static const TArray<FIntPoint> T = { {0, 1}, {1, 1}, {2, 1}, {1, 0} };
			static const TArray<FIntPoint> S = { {0, 1}, {1, 1}, {1, 0}, {2, 0} };
			static const TArray<FIntPoint> Z = { {0, 0}, {1, 0}, {1, 1}, {2, 1} };
			static const TArray<FIntPoint> J = { {0, 0}, {0, 1}, {1, 1}, {2, 1} };
			static const TArray<FIntPoint> L = { {2, 0}, {0, 1}, {1, 1}, {2, 1} };
			switch (InType)
			{
			case TEXT('I'): return I; case TEXT('O'): return O; case TEXT('T'): return T;
			case TEXT('S'): return S; case TEXT('Z'): return Z; case TEXT('J'): return J;
			case TEXT('L'): return L; default: return T;
			}
		}

		static FLinearColor GetBaseColor(TCHAR InType)
		{
			switch (InType)
			{
			case TEXT('I'): return FLinearColor(0.05f, 0.86f, 0.91f);
			case TEXT('O'): return FLinearColor(1.00f, 0.88f, 0.08f);
			case TEXT('T'): return FLinearColor(0.72f, 0.25f, 0.92f);
			case TEXT('S'): return FLinearColor(0.30f, 0.85f, 0.23f);
			case TEXT('Z'): return FLinearColor(0.95f, 0.19f, 0.26f);
			case TEXT('J'): return FLinearColor(0.18f, 0.38f, 0.94f);
			case TEXT('L'): return FLinearColor(1.00f, 0.54f, 0.08f);
			default: return FLinearColor::White;
			}
		}

		TCHAR Type = TEXT('T');
		float IconHeight = 16.f;
	};

	class FTotorisMinoDecorator final : public FRichTextDecorator
	{
	public:
		explicit FTotorisMinoDecorator(URichTextBlock* InOwner) : FRichTextDecorator(InOwner) {}

		virtual bool Supports(const FTextRunParseResults& Run, const FString& Text) const override
		{
			const FTextRange* IdRange = Run.MetaData.Find(TEXT("id"));
			if (Run.Name != TEXT("mino") || !IdRange) return false;
			const FString Id = Text.Mid(IdRange->BeginIndex, IdRange->EndIndex - IdRange->BeginIndex);
			return Id.Len() == 1 && FString(TEXT("IOTSZJL")).Contains(Id);
		}

	protected:
		virtual TSharedPtr<SWidget> CreateDecoratorWidget(
			const FTextRunInfo& RunInfo, const FTextBlockStyle& Style) const override
		{
			const FString* Id = RunInfo.MetaData.Find(TEXT("id"));
			if (!Id || Id->IsEmpty()) return nullptr;
			return SNew(STotorisInlineMino)
				.Type((*Id)[0])
				.IconHeight(FMath::Max(10.f, Style.Font.Size * .90f));
		}
	};
}

TSharedPtr<ITextDecorator> UTotorisMinoRichTextDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShared<FTotorisMinoDecorator>(InOwner);
}
