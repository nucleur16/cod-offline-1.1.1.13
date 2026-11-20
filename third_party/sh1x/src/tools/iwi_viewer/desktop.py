"""Native Windows texture viewer; no web server or browser."""
import argparse
import os
import sys
from pathlib import Path

from PySide6.QtCore import (QAbstractListModel, QModelIndex, QObject, QRunnable,
                           QSize, Qt, QSortFilterProxyModel, QThreadPool, QTimer, Signal)
from PySide6.QtGui import QAction, QBrush, QColor, QImage, QPainter, QPixmap
from PySide6.QtWidgets import (QApplication, QComboBox, QFileDialog, QGraphicsScene,
    QGraphicsView, QHBoxLayout, QLabel, QLineEdit, QListView, QMainWindow, QMessageBox,
    QPushButton, QSplitter, QVBoxLayout, QWidget)

from iwi import DEFAULT_ROOT, IMAGE_EXTENSIONS, decode_image, scan_images


class Signals(QObject):
    done = Signal(object)


class Job(QRunnable):
    def __init__(self, fn, callback):
        super().__init__()
        self.fn = fn
        self.signals = Signals()
        self.signals.done.connect(callback)

    def run(self):
        try:
            self.signals.done.emit((True, self.fn()))
        except Exception as exc:
            self.signals.done.emit((False, str(exc)))


class Files(QAbstractListModel):
    def __init__(self):
        super().__init__()
        self.items = []

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self.items)

    def data(self, index, role=Qt.DisplayRole):
        if index.isValid() and role in (Qt.DisplayRole, Qt.ToolTipRole):
            return self.items[index.row()][1]

    def replace(self, items):
        self.beginResetModel()
        self.items = items
        self.endResetModel()


class Preview(QGraphicsView):
    def __init__(self):
        super().__init__()
        self.setScene(QGraphicsScene(self))
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setRenderHint(QPainter.SmoothPixmapTransform)
        self.fit = True
        tile = QPixmap(24, 24)
        tile.fill(QColor('#252a31'))
        p = QPainter(tile)
        p.fillRect(0, 0, 12, 12, QColor('#353c46'))
        p.fillRect(12, 12, 12, 12, QColor('#353c46'))
        p.end()
        self.setBackgroundBrush(QBrush(tile))

    def show_image(self, image):
        self.scene().clear()
        rgba = image.convert('RGBA')
        # Explicit Qt bridge works in frozen builds without Pillow's optional
        # Qt-binding discovery (which PyInstaller intentionally excludes).
        qimage = QImage(rgba.tobytes(), rgba.width, rgba.height,
                        rgba.width * 4, QImage.Format_RGBA8888).copy()
        pixmap = QPixmap.fromImage(qimage)
        self.scene().addPixmap(pixmap)
        self.scene().setSceneRect(0, 0, image.width, image.height)
        if self.fit:
            self.fitInView(self.sceneRect(), Qt.KeepAspectRatio)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.fit and self.scene().items():
            self.fitInView(self.sceneRect(), Qt.KeepAspectRatio)

    def wheelEvent(self, event):
        self.fit = False
        factor = 1.2 if event.angleDelta().y() > 0 else 1 / 1.2
        scale = self.transform().m11() * factor
        if 0.01 <= scale <= 64:
            self.scale(factor, factor)
        event.accept()


class Window(QMainWindow):
    def __init__(self, root):
        super().__init__()
        self.setWindowTitle('sh1x IWI Viewer — Call of Duty Online')
        self.resize(1240, 820)
        self.pool = QThreadPool(self)
        self.pool.setMaxThreadCount(3)
        self.jobs = set()
        self.image = None
        self.current = None
        self.token = 0
        self.scan_token = 0
        self.setAcceptDrops(True)
        self.model = Files()
        self.proxy = QSortFilterProxyModel(self)
        self.proxy.setSourceModel(self.model)
        self.proxy.setFilterCaseSensitivity(Qt.CaseInsensitive)
        self.search = QLineEdit()
        self.search.setPlaceholderText('Search texture name or archive…')
        self.search.textChanged.connect(self.filter)
        self.list = QListView()
        self.list.setModel(self.proxy)
        self.list.setUniformItemSizes(True)
        self.list.setSpacing(3)
        self.list.selectionModel().currentChanged.connect(self.select)
        self.count = QLabel('Indexing…')
        left = QWidget()
        ll = QVBoxLayout(left)
        ll.addWidget(QLabel('TEXTURE LIBRARY'))
        ll.addWidget(self.search)
        ll.addWidget(self.count)
        ll.addWidget(self.list)
        right = QWidget()
        rl = QVBoxLayout(right)
        self.title = QLabel('Select an image')
        self.title.setStyleSheet('font-size: 20px; font-weight: 600; color: #b5e995')
        self.title.setWordWrap(True)
        self.path_label = QLabel('Open a folder or image, or drop either here. All subfolders are included.')
        self.path_label.setWordWrap(True)
        self.path_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        # Treat file names as plain text, including names with angle brackets.
        self.title.setTextFormat(Qt.PlainText)
        self.path_label.setTextFormat(Qt.PlainText)
        rl.addWidget(self.title)
        rl.addWidget(self.path_label)
        toolbar = QHBoxLayout()
        self.zoom = QComboBox()
        self.zoom.addItems(['Fit', '100%', '200%', '400%'])
        self.zoom.currentIndexChanged.connect(self.set_zoom)
        self.channel = QComboBox()
        self.channel.addItems(['Color + alpha', 'Color only', 'Alpha mask'])
        self.channel.currentIndexChanged.connect(self.render)
        self.face = QComboBox()
        self.face.hide()
        self.face.currentIndexChanged.connect(self.change_face)
        self.pixel = QPushButton('Nearest pixels')
        self.pixel.setCheckable(True)
        self.pixel.toggled.connect(lambda on: self.preview.setRenderHint(QPainter.SmoothPixmapTransform, not on))
        for w in (self.zoom, self.channel, self.face, self.pixel):
            toolbar.addWidget(w)
        toolbar.addStretch()
        rl.addLayout(toolbar)
        self.preview = Preview()
        rl.addWidget(self.preview, 1)
        self.meta = QLabel('Full-resolution PNG export preserves transparency.')
        self.meta.setWordWrap(True)
        rl.addWidget(self.meta)
        self.export = QPushButton('Export PNG…')
        self.export.setEnabled(False)
        self.export.clicked.connect(self.save)
        rl.addWidget(self.export)
        split = QSplitter()
        split.addWidget(left)
        split.addWidget(right)
        split.setSizes([420, 820])
        self.setCentralWidget(split)
        menu = self.menuBar().addMenu('&File')
        for name, shortcut, fn in [('Open &folder…', 'Ctrl+O', self.open_folder), ('Open &image…', 'Ctrl+Shift+O', self.open_file), ('Export &PNG…', 'Ctrl+S', self.save)]:
            action = QAction(name, self)
            action.setShortcut(shortcut)
            action.triggered.connect(fn)
            menu.addAction(action)
        self.statusBar().showMessage('Mouse wheel: zoom · Drag: pan · Arrow keys: browse')
        QTimer.singleShot(0, lambda: self.load_root(Path(root)))

    def work(self, fn, callback):
        job = Job(fn, callback)
        self.jobs.add(job)
        job.signals.done.connect(lambda _: self.jobs.discard(job))
        self.pool.start(job)

    def filter(self, text):
        self.proxy.setFilterFixedString(text)
        self.count.setText(f'{self.proxy.rowCount():,} / {self.model.rowCount():,} images · all subfolders')

    def load_root(self, root):
        self.scan_token += 1
        generation = self.scan_token
        self.count.setText('Indexing texture folders…')
        def done(result):
            if generation != self.scan_token:
                return
            ok, value = result
            if not ok:
                self.count.setText('Could not open folder')
                QMessageBox.warning(self, 'Open folder', value)
                return
            self.search.clear()
            self.model.replace(value)
            self.filter('')
            if value:
                self.list.setCurrentIndex(self.proxy.index(0, 0))
        self.work(lambda: scan_images(root), done)

    def select(self, index, _=None):
        if index.isValid():
            path = self.model.items[self.proxy.mapToSource(index).row()][0]
            self.load_image(path)

    def load_image(self, path, face=0):
        self.token += 1
        generation = self.token
        self.current = path
        self.image = None
        self.export.setEnabled(False)
        self.preview.scene().clear()
        self.title.setText(path.name)
        self.path_label.setText(str(path))
        self.meta.setText('Decoding…')
        def done(result):
            if generation != self.token:
                return
            ok, value = result
            if not ok:
                self.meta.setText('Cannot preview: ' + value)
                self.face.hide()
                return
            self.image, meta = value
            self.face.blockSignals(True)
            self.face.clear()
            self.face.addItems(['+X', '−X', '+Y', '−Y', '+Z', '−Z'] if meta['faces'] == 6 else [f'Slice {i}' for i in range(meta['faces'])])
            self.face.setCurrentIndex(face)
            self.face.setVisible(meta['faces'] > 1)
            self.face.blockSignals(False)
            self.meta.setText(f"{meta['width']} × {meta['height']}   |   {meta['format']}   |   {meta['bytes'] / 1024:,.1f} KB   |   PNG exports original colors and alpha")
            self.render()
            self.export.setEnabled(True)
        self.work(lambda: decode_image(path, face), done)

    def change_face(self, face):
        if self.current and face >= 0:
            self.load_image(self.current, face)

    def render(self, *_):
        if self.image is None:
            return
        image = self.image
        if self.channel.currentIndex() == 1:
            image = image.convert('RGB')
        elif self.channel.currentIndex() == 2:
            image = image.getchannel('A').convert('RGB')
        self.preview.show_image(image)

    def set_zoom(self, index):
        self.preview.fit = index == 0
        self.preview.resetTransform()
        if index:
            self.preview.scale([1, 1, 2, 4][index], [1, 1, 2, 4][index])
        elif self.image:
            self.preview.fitInView(self.preview.sceneRect(), Qt.KeepAspectRatio)

    def save(self):
        if self.image is None:
            return
        suffix = f'_face{self.face.currentIndex()}' if self.face.count() > 1 else ''
        target, _ = QFileDialog.getSaveFileName(self, 'Export original texture as PNG', str(self.current.with_name(self.current.stem + suffix + '.png')), 'PNG image (*.png)')
        if target:
            if not target.lower().endswith('.png'):
                target += '.png'
            try:
                self.image.save(target, 'PNG')
                self.statusBar().showMessage(f'PNG saved: {target}')
            except OSError as exc:
                QMessageBox.warning(self, 'PNG export failed', str(exc))

    def open_folder(self):
        folder = QFileDialog.getExistingDirectory(self, 'Select image folder (includes all subfolders)', DEFAULT_ROOT)
        if folder:
            self.load_root(Path(folder))

    def open_file(self):
        filters = 'Images (' + ' '.join('*' + ext for ext in sorted(IMAGE_EXTENSIONS)) + ')'
        path, _ = QFileDialog.getOpenFileName(self, 'Open image', DEFAULT_ROOT, filters)
        if path:
            self.load_root(Path(path))

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event):
        for url in event.mimeData().urls():
            path = Path(url.toLocalFile())
            if path.is_dir() or path.suffix.lower() in IMAGE_EXTENSIONS:
                self.load_root(path)
                event.acceptProposedAction()
                break


def configure_app(app):
    app.setStyle('Fusion')
    app.setStyleSheet('''QWidget { background: #171b21; color: #e4e9f0; font: 10pt "Segoe UI"; }
        QLineEdit, QComboBox, QPushButton { background: #262d37; border: 1px solid #424c5b; border-radius: 5px; padding: 7px; }
        QPushButton:hover { background: #384554; } QPushButton:disabled { color: #777; }
        QListView { background: #11151b; border: 0; } QListView::item:selected { background: #384b36; color: #d9ffc3; }
        QGraphicsView { border: 1px solid #424c5b; } QSplitter::handle { background: #303842; }
        QMenu::item:selected { background: #384554; }''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', default=DEFAULT_ROOT)
    args = parser.parse_args()
    app = QApplication(sys.argv[:1])
    configure_app(app)
    window = Window(args.root)
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
