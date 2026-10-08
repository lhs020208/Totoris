#include "TotorisMission.h"

namespace
{
	const TCHAR* FindKoreanDescription(const TCHAR* Id)
	{
		// Keep the player-facing Korean text beside the mission definitions rather
		// than deriving it from the developer-facing English title at runtime.
		static const TMap<FName, const TCHAR*> Descriptions = {
			{ TEXT("F_Perform_3_Combo"), TEXT("3콤보를 하세요") },
			{ TEXT("F_Clear_2_Doubles"), TEXT("더블을 2회 하세요") },
			{ TEXT("F_Clear_Quad"), TEXT("쿼드를 1회 하세요") },
			{ TEXT("F_Clear_6_Lines"), TEXT("총 6줄을 제거하세요") },
			{ TEXT("F_Clear_Single_O_Piece"), TEXT("O미노로 싱글을 하세요") },
			{ TEXT("F_Clear_Double_O_Piece"), TEXT("O미노로 더블을 하세요") },
			{ TEXT("F_Clear_Double_SZ_Piece"), TEXT("S/Z미노로 더블을 하세요") },
			{ TEXT("F_Clear_Triple_LJ_Piece"), TEXT("L/J미노로 트리플을 하세요") },
			{ TEXT("F_Clear_3_Lines_Holding_I"), TEXT("I미노를 홀드한 상태로 총 3줄을 제거하세요") },
			{ TEXT("F_Use_Hold_8_Times"), TEXT("홀드를 8회 사용하세요") },
			{ TEXT("F_Rotate_20_Times"), TEXT("유효한 회전을 20회 하세요") },
			{ TEXT("F_Clear_2_Singles_In_Row"), TEXT("싱글을 2회 연속으로 하세요") },
			{ TEXT("F_Place_All_7_Types"), TEXT("7종류 미노를 각각 1개 이상 배치하세요") },
			{ TEXT("F_Clear_Unrotated_Piece"), TEXT("한 번도 회전하지 않은 미노로 라인을 클리어하세요") },
			{ TEXT("F_Clear_Piece_Touching_Wall"), TEXT("왼쪽 또는 오른쪽 벽에 붙은 미노로 라인을 클리어하세요") },
			{ TEXT("F_Place_Blocks_All_10_Columns"), TEXT("배치한 미노의 블록이 1~10열을 각각 최소 한 번씩 차지하게 하세요") },
			{ TEXT("F_Clear_Piece_From_Hold"), TEXT("홀드에서 꺼낸 미노로 라인을 클리어하세요") },

			{ TEXT("E_Perform_Any_Spin"), TEXT("아무 종류의 스핀을 하세요") },
			{ TEXT("E_Clear_T_Spin_Single"), TEXT("T스핀 싱글을 하세요") },
			{ TEXT("E_Clear_T_Spin_Double"), TEXT("T스핀 더블을 하세요") },
			{ TEXT("E_Clear_SZ_Spin"), TEXT("S/Z 스핀으로 줄을 제거하세요") },
			{ TEXT("E_Clear_LJ_Spin"), TEXT("L/J 스핀으로 줄을 제거하세요") },
			{ TEXT("E_Perform_5_Combo"), TEXT("5콤보를 하세요") },
			{ TEXT("E_Clear_2_Lines_Horizontal_I"), TEXT("가로 I미노를 이용해 2줄을 제거하세요") },
			{ TEXT("E_Place_20_Pieces"), TEXT("미노를 20개 배치하세요") },
			{ TEXT("E_Send_6_Attack"), TEXT("공격량을 6 생성하세요") },
			{ TEXT("E_Place_2_O_Pieces_In_Row"), TEXT("O미노를 2개 연속으로 배치하세요") },
			{ TEXT("E_Place_12_Counterclockwise_Only"), TEXT("반시계 회전만 사용하며 12개를 배치하세요") },
			{ TEXT("E_Clear_6_Singles_No_Combo"), TEXT("콤보를 만들지 않으면서 싱글을 6회 하세요") },
			{ TEXT("E_5_Columns_Same_Height"), TEXT("연속된 5개 열의 높이를 동일하게 만드세요") },
			{ TEXT("E_Place_Vertical_I_Open_Sides"), TEXT("I미노를 세로로 배치하되 양옆이 블록이나 벽에 닿지 않게 하세요") },

			{ TEXT("D_Clear_4_Doubles"), TEXT("더블을 4회 하세요") },
			{ TEXT("D_Place_3_No_Move_No_Rotate"), TEXT("이동·회전 없이 미노를 3개 연속으로 배치하세요") },
			{ TEXT("D_Place_14_No_Line_Clear"), TEXT("줄을 하나도 지우지 않고 14개를 연속으로 배치하세요") },
			{ TEXT("D_Clear_2_Doubles_SZ"), TEXT("S/Z미노로 더블을 2회 하세요") },
			{ TEXT("D_Clear_2_Triples_LJ"), TEXT("L/J미노로 트리플을 2회 하세요") },
			{ TEXT("D_Clear_I_Spin"), TEXT("I스핀으로 줄을 제거하세요") },
			{ TEXT("D_Clear_Quad_Upper_Half"), TEXT("보드 상단 절반에서 쿼드를 하세요") },
			{ TEXT("D_Rotate_80_Times"), TEXT("유효한 회전을 80회 하세요") },
			{ TEXT("D_Clear_Quad_On_2_Combo"), TEXT("2콤보 이상인 상태에서 쿼드를 하세요") },
			{ TEXT("D_Clear_2_Singles_In_Row_SZ"), TEXT("S/Z미노로 싱글을 2회 연속으로 하세요") },
			{ TEXT("D_Perform_3_Combo_No_Hold"), TEXT("홀드 없이 3콤보를 하세요") },
			{ TEXT("D_Perform_3_Nonclearing_Spins"), TEXT("줄을 지우지 않는 스핀을 3회 하세요") },
			{ TEXT("D_Perform_2_SZLJ_Spins"), TEXT("S/Z/L/J 중 하나를 이용한 스핀을 2회 하세요") },
			{ TEXT("D_Clear_Single_Double_Triple"), TEXT("싱글·더블·트리플을 각각 1회 클리어하세요. 순서는 상관없습니다") },
			{ TEXT("D_Alternate_Clear_Not_Clear_6"), TEXT("클리어와 비클리어를 번갈아 하며 미노를 6개 배치하세요") },
			{ TEXT("D_Clear_Line_Exactly_2_Colors"), TEXT("정확히 두 종류의 색만 포함된 한 줄을 클리어하세요") },

			{ TEXT("C_Clear_T_Spin_Triple"), TEXT("T스핀 트리플을 하세요") },
			{ TEXT("C_Place_25_No_Hold"), TEXT("홀드 없이 25개를 연속으로 배치하세요") },
			{ TEXT("C_Clear_3_Triples"), TEXT("트리플을 3회 하세요") },
			{ TEXT("C_Reach_B2B_4"), TEXT("B2B x4에 도달하세요") },
			{ TEXT("C_Clear_Quad_2_Columns"), TEXT("서로 다른 두 열을 우물로 사용해 쿼드를 하세요") },
			{ TEXT("C_Use_Hold_12_In_Row"), TEXT("12개 미노를 연속으로 홀드하세요") },
			{ TEXT("C_Place_10_Continuous_Soft_Drop"), TEXT("소프트드롭 입력을 놓지 않고 10개를 배치하세요") },
			{ TEXT("C_Stack_Top_3_Rows_3_Seconds"), TEXT("스택 일부를 최상단 3줄에 3초간 유지하세요") },
			{ TEXT("C_Clear_10_No_T_or_I"), TEXT("T/I미노로 줄을 지우지 않고 총 10줄을 제거하세요") },
			{ TEXT("C_Clear_SZ_Spin_Triple"), TEXT("S/Z 스핀 트리플을 하세요") },
			{ TEXT("C_Clear_2_O_Doubles_In_Row"), TEXT("O미노로 더블을 2회 연속으로 하세요") },
			{ TEXT("C_Clear_4_T_Spin_Minis"), TEXT("T스핀 미니 클리어를 4회 하세요") },
			{ TEXT("C_Send_14_Attack"), TEXT("공격량을 14 생성하세요") },
			{ TEXT("C_Clear_3_Doubles_Same_Piece"), TEXT("같은 종류의 미노로 더블을 3회 하세요") },
			{ TEXT("C_Clear_Garbage_LJ_Spin"), TEXT("L/J 스핀으로 가비지 줄을 제거하세요") },
			{ TEXT("C_Clear_Garbage_SZ_Spin"), TEXT("S/Z 스핀으로 가비지 줄을 제거하세요") },
			{ TEXT("C_Place_3_O_Column_1"), TEXT("1열에 O미노를 3개 배치하세요") },
			{ TEXT("C_Clear_2_Spins_One_Combo"), TEXT("하나의 콤보 안에서 스핀 클리어를 2회 하세요") },
			{ TEXT("C_Clear_Single_I_No_Move_Rotate"), TEXT("이동·회전시키지 않은 I미노로 싱글을 하세요") },
			{ TEXT("C_Place_6_No_DAS_Release"), TEXT("DAS 입력을 놓지 않고 미노를 6개 배치하세요") },
			{ TEXT("C_Clear_Lines_All_7_Types"), TEXT("7종류 미노로 각각 최소 1번 라인을 클리어하세요") },
			{ TEXT("C_Stack_At_Or_Above_Row_22"), TEXT("22행 이상의 위치에 블록이 존재하게 하세요") },

			{ TEXT("B_Clear_6_Lines_O_Pieces"), TEXT("O미노로 총 6줄을 제거하세요") },
			{ TEXT("B_Spin_Clears_3_Pieces"), TEXT("서로 다른 3종류의 미노로 스핀 클리어를 하세요") },
			{ TEXT("B_Clear_4_Quads"), TEXT("쿼드를 4회 하세요") },
			{ TEXT("B_Place_5_No_Move_Rotate"), TEXT("이동·회전 없이 5개를 연속으로 배치하세요") },
			{ TEXT("B_Clear_LJ_Spin_Triple"), TEXT("L/J 스핀 트리플을 하세요") },
			{ TEXT("B_Clear_2_Quads_In_Row"), TEXT("쿼드를 2회 연속으로 하세요") },
			{ TEXT("B_Clear_8_Singles_Only"), TEXT("다른 클리어나 홀드 없이 싱글을 8회 하세요") },
			{ TEXT("B_No_Garbage_4_Seconds"), TEXT("보드에 가비지가 전혀 없는 상태를 4초간 유지하세요") },
			{ TEXT("B_Rotate_300_Times"), TEXT("유효한 회전을 300회 하세요") },
			{ TEXT("B_Dont_Cancel_Garbage_8_Seconds"), TEXT("8초 동안 가비지를 하나도 상쇄하지 마세요") },
			{ TEXT("B_T_Spin_Double_Up"), TEXT("T미노가 위쪽을 향한 상태로 TSD를 하세요") },
			{ TEXT("B_Clear_Double_O_No_Move_Rotate"), TEXT("이동·회전 없이 떨어뜨린 O미노로 더블을 하세요") },
			{ TEXT("B_Place_3_T_No_Rotate"), TEXT("회전하지 않고 T미노를 3개 배치하세요") },
			{ TEXT("B_T_Spin_Double_On_2_Combo"), TEXT("2콤보 이상인 상태에서 TSD를 하세요") },
			{ TEXT("B_Clear_Line_One_Color"), TEXT("한 줄의 10칸을 전부 동일한 색 블록으로 구성해 클리어하세요") },
			{ TEXT("B_Clear_Single_Double_Triple_Quad"), TEXT("싱글 → 더블 → 트리플 → 쿼드 순으로 클리어하세요") },
			{ TEXT("B_Clear_Quads_Both_Edge_Columns"), TEXT("1열을 우물로 쿼드를 1회, 10열을 우물로 쿼드를 1회 하세요") },
			{ TEXT("B_Clear_Line_5_Colors"), TEXT("한 줄 안에 서로 다른 5종류 미노 색이 포함된 상태로 클리어하세요") },
			{ TEXT("B_Clear_2_Non_Adjacent_Lines"), TEXT("서로 붙어 있지 않은 두 줄을 한 번의 미노 배치로 동시에 클리어하세요") },
			{ TEXT("B_Clear_Line_Row_18_Or_Above"), TEXT("18행 이상의 높이에서 라인을 클리어하세요") },

			{ TEXT("A_Perform_7_Combo"), TEXT("7콤보를 하세요") },
			{ TEXT("A_Clear_I_Spin_Double"), TEXT("I스핀 더블을 하세요") },
			{ TEXT("A_Clear_2_SZ_Spin_Doubles"), TEXT("S/Z 스핀 더블을 2회 연속으로 하세요") },
			{ TEXT("A_Clear_2_LJ_Spin_Doubles"), TEXT("L/J 스핀 더블을 2회 연속으로 하세요") },
			{ TEXT("A_Perform_Color_Clear"), TEXT("가비지를 제외한 모든 일반 미노를 제거하세요") },
			{ TEXT("A_Clear_40_Lines"), TEXT("총 40줄을 제거하세요") },
			{ TEXT("A_Clear_4_Spins_One_Combo"), TEXT("하나의 콤보 안에서 스핀 클리어를 4회 하세요") },
			{ TEXT("A_T_Spin_Double_Triple_Edge"), TEXT("중심이 1열 또는 10열인 TSD/TST를 하세요") },
			{ TEXT("A_Reach_B2B_10"), TEXT("B2B x10에 도달하세요") },
			{ TEXT("A_Clear_Line_All_7_Colors"), TEXT("한 줄에 7종류 미노 색을 모두 포함시켜 클리어하세요") },
			{ TEXT("A_Perform_Perfect_Clear"), TEXT("필드의 모든 블록을 제거하는 퍼펙트 클리어를 하세요") }
		};

		if (const TCHAR* const* Description = Descriptions.Find(FName(Id)))
		{
			return *Description;
		}

		return TEXT("");
	}

	FTotorisMissionDefinition Mission(const TCHAR* Id, ETotorisMissionTier Tier,
		const TCHAR* Text)
	{
		FTotorisMissionDefinition Result;
		Result.Id = FName(Id);
		Result.Tier = Tier;
		Result.DisplayText = FText::FromString(Text);
		Result.Description = FText::FromString(FindKoreanDescription(Id));
		return Result;
	}

	const TArray<FTotorisMissionDefinition>& BuildAll()
	{
		static const TArray<FTotorisMissionDefinition> Missions = {
			// F tier
			Mission(TEXT("F_Perform_3_Combo"), ETotorisMissionTier::F, TEXT("Perform a 3-Combo")),
			Mission(TEXT("F_Clear_2_Doubles"), ETotorisMissionTier::F, TEXT("Clear 2 Doubles")),
			Mission(TEXT("F_Clear_Quad"), ETotorisMissionTier::F, TEXT("Clear a Quad")),
			Mission(TEXT("F_Clear_6_Lines"), ETotorisMissionTier::F, TEXT("Clear 6 Lines")),
			Mission(TEXT("F_Clear_Single_O_Piece"), ETotorisMissionTier::F, TEXT("Clear a Single using an O-Piece")),
			Mission(TEXT("F_Clear_Double_O_Piece"), ETotorisMissionTier::F, TEXT("Clear a Double using an O-Piece")),
			Mission(TEXT("F_Clear_Double_SZ_Piece"), ETotorisMissionTier::F, TEXT("Clear a Double using an S or Z-Piece")),
			Mission(TEXT("F_Clear_Triple_LJ_Piece"), ETotorisMissionTier::F, TEXT("Clear a Triple using an L or J-Piece")),
			Mission(TEXT("F_Clear_3_Lines_Holding_I"), ETotorisMissionTier::F, TEXT("Clear 3 lines while holding an I-Piece")),
			Mission(TEXT("F_Use_Hold_8_Times"), ETotorisMissionTier::F, TEXT("Use Hold 8 times")),
			Mission(TEXT("F_Rotate_20_Times"), ETotorisMissionTier::F, TEXT("Rotate 20 times")),
			Mission(TEXT("F_Clear_2_Singles_In_Row"), ETotorisMissionTier::F, TEXT("Clear 2 Singles in a row")),
			Mission(TEXT("F_Place_All_7_Types"), ETotorisMissionTier::F, TEXT("Place all 7 Tetromino types")),
			Mission(TEXT("F_Clear_Unrotated_Piece"), ETotorisMissionTier::F, TEXT("Clear a Line using an unrotated Piece")),
			Mission(TEXT("F_Clear_Piece_Touching_Wall"), ETotorisMissionTier::F, TEXT("Clear a Line with a Piece touching either Wall")),
			Mission(TEXT("F_Place_Blocks_All_10_Columns"), ETotorisMissionTier::F, TEXT("Place Pieces in all 10 Columns")),
			Mission(TEXT("F_Clear_Piece_From_Hold"), ETotorisMissionTier::F, TEXT("Clear a Line using a Piece taken from Hold")),

			// E tier
			Mission(TEXT("E_Perform_Any_Spin"), ETotorisMissionTier::E, TEXT("Perform any Spin")),
			Mission(TEXT("E_Clear_T_Spin_Single"), ETotorisMissionTier::E, TEXT("Clear a T-Spin Single")),
			Mission(TEXT("E_Clear_T_Spin_Double"), ETotorisMissionTier::E, TEXT("Clear a T-Spin Double")),
			Mission(TEXT("E_Clear_SZ_Spin"), ETotorisMissionTier::E, TEXT("Clear an S/Z-Spin")),
			Mission(TEXT("E_Clear_LJ_Spin"), ETotorisMissionTier::E, TEXT("Clear an L/J-Spin")),
			Mission(TEXT("E_Perform_5_Combo"), ETotorisMissionTier::E, TEXT("Perform a 5-Combo")),
			Mission(TEXT("E_Clear_2_Lines_Horizontal_I"), ETotorisMissionTier::E, TEXT("Clear 2 Lines using horizontal I-Pieces")),
			Mission(TEXT("E_Place_20_Pieces"), ETotorisMissionTier::E, TEXT("Place 20 pieces")),
			Mission(TEXT("E_Send_6_Attack"), ETotorisMissionTier::E, TEXT("Send 6 Attack")),
			Mission(TEXT("E_Place_2_O_Pieces_In_Row"), ETotorisMissionTier::E, TEXT("Place 2 O-Pieces in a row")),
			Mission(TEXT("E_Place_12_Counterclockwise_Only"), ETotorisMissionTier::E, TEXT("Place 12 pieces while only rotating counterclockwise")),
			Mission(TEXT("E_Clear_6_Singles_No_Combo"), ETotorisMissionTier::E, TEXT("Clear 6 Singles without starting a combo")),
			Mission(TEXT("E_5_Columns_Same_Height"), ETotorisMissionTier::E, TEXT("Make 5 Consecutive Columns the Same Height")),
			Mission(TEXT("E_Place_Vertical_I_Open_Sides"), ETotorisMissionTier::E, TEXT("Place a vertical I-Piece without touching Blocks or Walls on either side")),

			// D tier
			Mission(TEXT("D_Clear_4_Doubles"), ETotorisMissionTier::D, TEXT("Clear 4 Doubles")),
			Mission(TEXT("D_Place_3_No_Move_No_Rotate"), ETotorisMissionTier::D, TEXT("Place 3 pieces in a row without moving or rotating")),
			Mission(TEXT("D_Place_14_No_Line_Clear"), ETotorisMissionTier::D, TEXT("Place 14 pieces in a row without clearing any lines")),
			Mission(TEXT("D_Clear_2_Doubles_SZ"), ETotorisMissionTier::D, TEXT("Clear 2 Doubles using S or Z-Pieces")),
			Mission(TEXT("D_Clear_2_Triples_LJ"), ETotorisMissionTier::D, TEXT("Clear 2 Triples using L or J-Pieces")),
			Mission(TEXT("D_Clear_I_Spin"), ETotorisMissionTier::D, TEXT("Clear an I-Spin")),
			Mission(TEXT("D_Clear_Quad_Upper_Half"), ETotorisMissionTier::D, TEXT("Clear a Quad in the upper half of the board")),
			Mission(TEXT("D_Rotate_80_Times"), ETotorisMissionTier::D, TEXT("Rotate 80 times")),
			Mission(TEXT("D_Clear_Quad_On_2_Combo"), ETotorisMissionTier::D, TEXT("Clear a Quad while on a 2+-Combo")),
			Mission(TEXT("D_Clear_2_Singles_In_Row_SZ"), ETotorisMissionTier::D, TEXT("Clear 2 Singles in a row using S or Z-Pieces")),
			Mission(TEXT("D_Perform_3_Combo_No_Hold"), ETotorisMissionTier::D, TEXT("Perform a 3-Combo without using Hold")),
			Mission(TEXT("D_Perform_3_Nonclearing_Spins"), ETotorisMissionTier::D, TEXT("Perform 3 Spins that don't clear any lines")),
			Mission(TEXT("D_Perform_2_SZLJ_Spins"), ETotorisMissionTier::D, TEXT("Perform 2 S/Z/L/J-Spins")),
			Mission(TEXT("D_Clear_Single_Double_Triple"), ETotorisMissionTier::D, TEXT("Clear a Single, Double and Triple")),
			Mission(TEXT("D_Alternate_Clear_Not_Clear_6"), ETotorisMissionTier::D, TEXT("Alternate between Clearing and Not Clearing for 6 Pieces")),
			Mission(TEXT("D_Clear_Line_Exactly_2_Colors"), ETotorisMissionTier::D, TEXT("Clear a Line containing exactly 2 Colors")),

			// C tier
			Mission(TEXT("C_Clear_T_Spin_Triple"), ETotorisMissionTier::C, TEXT("Clear a T-Spin Triple")),
			Mission(TEXT("C_Place_25_No_Hold"), ETotorisMissionTier::C, TEXT("Place 25 pieces in a row without using Hold")),
			Mission(TEXT("C_Clear_3_Triples"), ETotorisMissionTier::C, TEXT("Clear 3 Triples")),
			Mission(TEXT("C_Reach_B2B_4"), ETotorisMissionTier::C, TEXT("Reach B2B x4")),
			Mission(TEXT("C_Clear_Quad_2_Columns"), ETotorisMissionTier::C, TEXT("Clear a Quad in 2 different columns")),
			Mission(TEXT("C_Use_Hold_12_In_Row"), ETotorisMissionTier::C, TEXT("Use Hold on 12 pieces in a row")),
			Mission(TEXT("C_Place_10_Continuous_Soft_Drop"), ETotorisMissionTier::C, TEXT("Place 10 pieces without releasing Soft Drop")),
			Mission(TEXT("C_Stack_Top_3_Rows_3_Seconds"), ETotorisMissionTier::C, TEXT("Have part of your stack in the top 3 rows for 3 seconds")),
			Mission(TEXT("C_Clear_10_No_T_or_I"), ETotorisMissionTier::C, TEXT("Clear 10 Lines without clearing with T or I-Pieces")),
			Mission(TEXT("C_Clear_SZ_Spin_Triple"), ETotorisMissionTier::C, TEXT("Clear an S/Z-Spin Triple")),
			Mission(TEXT("C_Clear_2_O_Doubles_In_Row"), ETotorisMissionTier::C, TEXT("Clear 2 Doubles consecutively using two O-Pieces")),
			Mission(TEXT("C_Clear_4_T_Spin_Minis"), ETotorisMissionTier::C, TEXT("Clear 4 T-Spin Minis")),
			Mission(TEXT("C_Send_14_Attack"), ETotorisMissionTier::C, TEXT("Send 14 Attack")),
			Mission(TEXT("C_Clear_3_Doubles_Same_Piece"), ETotorisMissionTier::C, TEXT("Clear 3 Doubles with the same type of piece")),
			Mission(TEXT("C_Clear_Garbage_LJ_Spin"), ETotorisMissionTier::C, TEXT("Clear Garbage using a L/J-Spin")),
			Mission(TEXT("C_Clear_Garbage_SZ_Spin"), ETotorisMissionTier::C, TEXT("Clear Garbage using a S/Z-Spin")),
			Mission(TEXT("C_Place_3_O_Column_1"), ETotorisMissionTier::C, TEXT("Place 3 O-Pieces in column 1")),
			Mission(TEXT("C_Clear_2_Spins_One_Combo"), ETotorisMissionTier::C, TEXT("Clear 2 Spins in one combo")),
			Mission(TEXT("C_Clear_Single_I_No_Move_Rotate"), ETotorisMissionTier::C, TEXT("Clear a Single with an I-Piece without moving or rotating")),
			Mission(TEXT("C_Place_6_No_DAS_Release"), ETotorisMissionTier::C, TEXT("Place 6 Pieces without releasing DAS")),
			Mission(TEXT("C_Clear_Lines_All_7_Types"), ETotorisMissionTier::C, TEXT("Clear Lines using all 7 Tetromino types")),
			Mission(TEXT("C_Stack_At_Or_Above_Row_22"), ETotorisMissionTier::C, TEXT("Have part of your Stack at or above Row 22")),

			// B tier
			Mission(TEXT("B_Clear_6_Lines_O_Pieces"), ETotorisMissionTier::B, TEXT("Clear 6 Lines using O-Pieces")),
			Mission(TEXT("B_Spin_Clears_3_Pieces"), ETotorisMissionTier::B, TEXT("Clear Spin-Clears with 3 different pieces")),
			Mission(TEXT("B_Clear_4_Quads"), ETotorisMissionTier::B, TEXT("Clear 4 Quads")),
			Mission(TEXT("B_Place_5_No_Move_Rotate"), ETotorisMissionTier::B, TEXT("Place 5 pieces in a row without moving or rotating")),
			Mission(TEXT("B_Clear_LJ_Spin_Triple"), ETotorisMissionTier::B, TEXT("Clear an L/J-Spin Triple")),
			Mission(TEXT("B_Clear_2_Quads_In_Row"), ETotorisMissionTier::B, TEXT("Clear 2 Quads in a row")),
			Mission(TEXT("B_Clear_8_Singles_Only"), ETotorisMissionTier::B, TEXT("Clear 8 Singles without doing other clears or using Hold")),
			Mission(TEXT("B_No_Garbage_4_Seconds"), ETotorisMissionTier::B, TEXT("Have no Garbage Lines on your board for 4 seconds")),
			Mission(TEXT("B_Rotate_300_Times"), ETotorisMissionTier::B, TEXT("Rotate 300 times")),
			Mission(TEXT("B_Dont_Cancel_Garbage_8_Seconds"), ETotorisMissionTier::B, TEXT("Don't cancel any garbage for 8 seconds")),
			Mission(TEXT("B_T_Spin_Double_Up"), ETotorisMissionTier::B, TEXT("Clear a T-Spin Double with the Piece pointing up")),
			Mission(TEXT("B_Clear_Double_O_No_Move_Rotate"), ETotorisMissionTier::B, TEXT("Clear a Double with an O-Piece without moving or rotating")),
			Mission(TEXT("B_Place_3_T_No_Rotate"), ETotorisMissionTier::B, TEXT("Place 3 T-Pieces without rotating any")),
			Mission(TEXT("B_T_Spin_Double_On_2_Combo"), ETotorisMissionTier::B, TEXT("Clear a T-Spin Double while on a 2+-Combo")),
			Mission(TEXT("B_Clear_Line_One_Color"), ETotorisMissionTier::B, TEXT("Clear a Line made entirely of one Color")),
			Mission(TEXT("B_Clear_Single_Double_Triple_Quad"), ETotorisMissionTier::B, TEXT("Clear a Single -> Double -> Triple -> Quad")),
			Mission(TEXT("B_Clear_Quads_Both_Edge_Columns"), ETotorisMissionTier::B, TEXT("Clear Quads using both Edge Columns")),
			Mission(TEXT("B_Clear_Line_5_Colors"), ETotorisMissionTier::B, TEXT("Clear a Line containing 5 different Colors")),
			Mission(TEXT("B_Clear_2_Non_Adjacent_Lines"), ETotorisMissionTier::B, TEXT("Clear 2 Non-Adjacent Lines at Once")),
			Mission(TEXT("B_Clear_Line_Row_18_Or_Above"), ETotorisMissionTier::B, TEXT("Clear a Line at or above Row 18")),

			// A tier
			Mission(TEXT("A_Perform_7_Combo"), ETotorisMissionTier::A, TEXT("Perform a 7-Combo")),
			Mission(TEXT("A_Clear_I_Spin_Double"), ETotorisMissionTier::A, TEXT("Clear an I-Spin Double")),
			Mission(TEXT("A_Clear_2_SZ_Spin_Doubles"), ETotorisMissionTier::A, TEXT("Clear two S/Z-Spin Doubles consecutively")),
			Mission(TEXT("A_Clear_2_LJ_Spin_Doubles"), ETotorisMissionTier::A, TEXT("Clear two L/J-Spin Doubles consecutively")),
			Mission(TEXT("A_Perform_Color_Clear"), ETotorisMissionTier::A, TEXT("Perform a Color Clear")),
			Mission(TEXT("A_Clear_40_Lines"), ETotorisMissionTier::A, TEXT("Clear 40 Lines")),
			Mission(TEXT("A_Clear_4_Spins_One_Combo"), ETotorisMissionTier::A, TEXT("Clear 4 Spins in one Combo")),
			Mission(TEXT("A_T_Spin_Double_Triple_Edge"), ETotorisMissionTier::A, TEXT("Clear a T-Spin Double/Triple centered in column 1 or 10")),
			Mission(TEXT("A_Reach_B2B_10"), ETotorisMissionTier::A, TEXT("Reach B2B x10")),
			Mission(TEXT("A_Clear_Line_All_7_Colors"), ETotorisMissionTier::A, TEXT("Clear a Line containing all 7 Colors")),
			Mission(TEXT("A_Perform_Perfect_Clear"), ETotorisMissionTier::A, TEXT("Perform a Perfect Clear"))
		};
		return Missions;
	}
}

namespace TotorisMissions
{
	const TArray<FTotorisMissionDefinition>& All()
	{
		return BuildAll();
	}

	const TArray<FTotorisMissionDefinition>& ForTier(ETotorisMissionTier Tier)
	{
		static TMap<ETotorisMissionTier, TArray<FTotorisMissionDefinition>> Cached;
		if (const TArray<FTotorisMissionDefinition>* Existing = Cached.Find(Tier))
		{
			return *Existing;
		}

		TArray<FTotorisMissionDefinition>& Result = Cached.Add(Tier);
		for (const FTotorisMissionDefinition& Definition : All())
		{
			if (Definition.Tier == Tier)
			{
				Result.Add(Definition);
			}
		}
		return Result;
	}
}
