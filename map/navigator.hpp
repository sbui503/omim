#pragma once
#include "../geometry/screenbase.hpp"

#include "../base/matrix.hpp"

#include "../base/start_mem_debug.hpp"


class FileReader;
template <class> class ReaderSource;
class FileWriter;

// Calculates screen parameters in navigation (dragging, scaling, etc.).
class Navigator
{
public:
  Navigator();
  explicit Navigator(ScreenBase const & screen);

  void SetMinScreenParams(unsigned pxMinWidth, double metresMinWidth);
  void SetFromRect(m2::RectD const & r);
  void CenterViewport(m2::PointD const & p);

  void SaveState();
  /// @return false if can't load previously saved values
  bool LoadState();

  void OnSize(int x0, int y0, int w, int h);

  ScreenBase const & Screen() const { return m_Screen; }
  ScreenBase const & StartScreen() const {return m_StartScreen; }
  /// Returns a matrix, which represents a transformation between two screens
  math::Matrix<float, 3, 3> const ScreenDelta(ScreenBase const & src, ScreenBase const & dst) const;

  void StartDrag(m2::PointD const & pt, double timeInSec);
  void DoDrag(m2::PointD const & pt, double timeInSec);
  void StopDrag(m2::PointD const & pt, double timeInSec, bool animate);

  void StartScale(m2::PointD const & pt1, m2::PointD const & pt2, double timeInSec);
  void DoScale(m2::PointD const & org, m2::PointD const & p1, m2::PointD const & p2);
  void DoScale(m2::PointD const & pt1, m2::PointD const & pt2, double timeInSec);
  void StopScale(m2::PointD const & pt1, m2::PointD const & pt2, double timeInSec);

  void ScaleToPoint(m2::PointD const & pt, double factor, double timeInSec);

  void Scale(double scale);
  void Rotate(double angle);

  void Move(double azDir, double factor);

  // Returns true if another update is necessary, i.e. animation is not finished.
  bool Update(double timeInSec);

  bool InAction() const;

  void SetOrientation(EOrientation orientation);
  EOrientation Orientation() const;

  // Do appropriate coordinate axes swapping according to orientation
  m2::PointD const OrientPoint(m2::PointD const & pt) const;


private:


  unsigned m_pxMinWidth;
  double m_metresMinWidth;
  bool CheckMinScale(ScreenBase const & screen);
  bool CheckMaxScale(ScreenBase const & screen);
  bool CheckBorders(ScreenBase const & screen);

  // Internal screen corresponding to the state when navigation began with StartDrag or StartScale.
  ScreenBase m_StartScreen;
  // Internal screen to do GtoP() and PtoG() calculations. It is always up to date with navigation.
  ScreenBase m_Screen;
  // Intial point for dragging and scaling.
  m2::PointD m_StartPt1;
  // Last point for dragging and scaling.
  m2::PointD m_LastPt1;
  // Second initial point for scaling.
  m2::PointD m_StartPt2;
  // Second Last point for scaling.
  m2::PointD m_LastPt2;
  // Current screen speed in post-dragging animation.
  double m_DragAnimationSpeed;
  // Move direction of the screen in post-dragging animation.
  double m_DragAnimationDirection;
  // Last update time.
  double m_LastUpdateTimeInSec;
  // Delta matrix which stores transformation between m_StartScreen and m_Screen.
  math::Matrix<float, 3, 3> m_DeltaMatrix;
  // Flag, which indicates, whether we are in the middle of some action.
  bool m_InAction;
  // Device orientation
  EOrientation m_orientation;
  // Used in DoScale and ScaleByPoint
  bool ScaleImpl(m2::PointD const & newPt1, m2::PointD const & newPt2, m2::PointD const & oldPt1, m2::PointD const & oldPt2, bool skipMaxScaleCheck);
};

#include "../base/stop_mem_debug.hpp"
