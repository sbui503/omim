#pragma once

#include "../geometry/rect2d.hpp"
#include "../geometry/point2d.hpp"

namespace scales
{
  inline int GetUpperScale() { return 17; }
  inline int GetUpperWorldScale() { return 9; }

  double GetM2PFactor(int level);

  double GetScaleLevelD(double ratio);
  double GetScaleLevelD(m2::RectD const & r);
  int GetScaleLevel(double ratio);
  int GetScaleLevel(m2::RectD const & r);

  /// @return such ration, that GetScaleLevel(ration) == level
  double GetRationForLevel(int level);
  /// @return such rect, that GetScaleLevel(rect) == level
  m2::RectD GetRectForLevel(int level, m2::PointD const & center, double X2YRatio);

  double GetEpsilonForLevel(int level);
  double GetEpsilonForSimplify(int level);
  bool IsGoodForLevel(int level, m2::RectD const & r);
}
