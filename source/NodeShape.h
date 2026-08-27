#pragma once
#include <memory>


typedef enum
{
	RECTANGULE_NODE = 0,
	DIAMOND_NODE = 1
}eNodeType;


class CNodeShape
{
public:
	virtual ~CNodeShape() {}
	virtual void Draw(CDC* pDC) = 0;
	virtual bool HitTest(CPoint pt) const = 0;
	virtual void MoveBy(int dx, int dy) = 0;
	virtual void MoveTo(int dx, int dy) = 0;

	virtual CPoint GetConnectionPoint(int index) const = 0;

	virtual std::unique_ptr<CNodeShape> Clone() const = 0;
};


class CDiamondNode : public CNodeShape
{
public:
	CPoint center;
	CString labelText;
	BOOL isSelected = false;

private:
	int m_X, m_Y, m_H, m_W;

public:
	CDiamondNode(int X, int Y, const CString& label);

	virtual void Draw(CDC* pDC) override;
	virtual bool HitTest(CPoint pt)const override;
	virtual void MoveBy(int dx, int dy) override;
	virtual void MoveTo(int dx, int dy) override;

	virtual CPoint GetConnectionPoint(int index) const override;

	std::unique_ptr<CNodeShape> Clone() const override { return std::make_unique<CDiamondNode>(*this); }
};


class CRectangleNode : public CNodeShape
{
public:
	CRect rect;
	CString labelText;
	BOOL isSelected = false;

private:
	int m_X, m_Y;

public:
	CRectangleNode(int X, int Y, const CString& label);

	virtual void Draw(CDC* pDC) override;
	virtual bool HitTest(CPoint pt) const override;
	virtual void MoveBy(int dx, int dy) override;
	virtual void MoveTo(int dx, int dy) override;

	virtual CPoint GetConnectionPoint(int index) const override;

	std::unique_ptr<CNodeShape> Clone() const override { return std::make_unique<CRectangleNode>(*this); }
};


class CNodeFactory
{
public:
	static std::unique_ptr<CNodeShape> GetNodeType(eNodeType nodeType, int X, int Y, const CString& label)
	{
		switch (nodeType)
		{
		case RECTANGULE_NODE:
			return std::make_unique<CRectangleNode>(X, Y, label);
		case DIAMOND_NODE:
			return std::make_unique<CDiamondNode>(X, Y, label);
		default:
			return nullptr;
		}
	}

};