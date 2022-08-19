#ifndef __CAMERA_NODE_H__
#define __CAMERA_NODE_H__

#include "zenonode.h"

class CameraNode : public ZenoNode
{
    Q_OBJECT
public:
    CameraNode(const NodeUtilParam& params, QGraphicsItem* parent = nullptr);
    ~CameraNode();

protected:
    QGraphicsLinearLayout* initCustomParamWidgets() override;

private slots:
    void onEditClicked();
};


#endif