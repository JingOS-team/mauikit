import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import org.kde.kirigami 2.2 as Kirigami

ItemDelegate
{
    width: parent.width
    height: rowHeight

    property bool isSection : false
    property bool boldLabel : false
    property alias label: labelTxt.text
    property alias labelTxt : labelTxt
    property string labelColor: ListView.isCurrentItem ? highlightedTextColor : textColor

    Rectangle
    {
        anchors.fill: parent
        color:  isSection ? viewBackgroundColor : (index % 2 === 0 ? Qt.darker(backgroundColor) : "transparent")
        opacity: 0.1
    }

    ColumnLayout
    {
        anchors.fill: parent

        Label
        {
            id: labelTxt
            Layout.margins: contentMargins
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

            width: parent.width
            height: parent.height

            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            text: labelTxt.text
            elide: Text.ElideRight
            color: labelColor
            font.pointSize: fontSizes.default

            font.bold: boldLabel
            font.weight : boldLabel ? Font.Bold : Font.Normal
        }
    }
}
