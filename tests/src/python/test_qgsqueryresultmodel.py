# -*- coding: utf-8 -*-
"""QGIS Unit tests for Postgres QgsQueryResultModel.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

"""
__author__ = 'Alessandro Pasotti'
__date__ = '24/12/2020'
__copyright__ = 'Copyright 2020, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '735cc85be9216e9681ca7b6a426e9ecb56cb5df2'

import os
from time import sleep
from qgis.core import (
    QgsProviderRegistry,
    QgsQueryResultModel,
    QgsAbstractDatabaseProviderConnection,
)
from qgis.testing import unittest, start_app
from qgis.PyQt.QtCore import QCoreApplication, QVariant, Qt, QTimer
from qgis.PyQt.QtWidgets import QListView, QDialog, QVBoxLayout, QLabel


class TestPyQgsQgsQueryResultModel(unittest.TestCase):

    NUM_RECORDS = 100050

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""

        QCoreApplication.setOrganizationName("QGIS_Test")
        QCoreApplication.setOrganizationDomain(cls.__name__)
        QCoreApplication.setApplicationName(cls.__name__)
        start_app()
        cls.postgres_conn = "service='qgis_test'"
        if 'QGIS_PGTEST_DB' in os.environ:
            cls.postgres_conn = os.environ['QGIS_PGTEST_DB']
        cls.uri = cls.postgres_conn + ' sslmode=disable'

        # Prepare data for threaded test
        cls._deleteBigData()

        md = QgsProviderRegistry.instance().providerMetadata('postgres')
        conn = md.createConnection(cls.uri, {})
        conn.executeSql('SELECT * INTO qgis_test.random_big_data FROM generate_series(1,%s) AS id, md5(random()::text) AS descr' % cls.NUM_RECORDS)

    @classmethod
    def tearDownClass(cls):

        cls._deleteBigData()

    @classmethod
    def _deleteBigData(cls):

        return
        try:
            md = QgsProviderRegistry.instance().providerMetadata('postgres')
            conn = md.createConnection(cls.uri, {})
            conn.dropVectorTable('qgis_test', 'random_big_data')
        except:
            pass

    def test_model(self):
        """Test the model"""

        md = QgsProviderRegistry.instance().providerMetadata('postgres')
        conn = md.createConnection(self.uri, {})
        res = conn.execSql('SELECT generate_series(1, 1000)')
        model = QgsQueryResultModel(res)
        self.assertEqual(model.rowCount(model.index(-1, -1)), 0)

        while model.rowCount(model.index(-1, -1)) < 1000:
            QCoreApplication.processEvents()

        self.assertEqual(model.columnCount(model.index(-1, -1)), 1)
        self.assertEqual(model.rowCount(model.index(-1, -1)), 1000)
        self.assertEqual(model.data(model.index(999, 0), Qt.DisplayRole), 1000)

        # Test data
        for i in range(1000):
            self.assertEqual(model.data(model.index(i, 0), Qt.DisplayRole), i + 1)

        self.assertEqual(model.data(model.index(1000, 0), Qt.DisplayRole), QVariant())
        self.assertEqual(model.data(model.index(1, 1), Qt.DisplayRole), QVariant())

    def test_model_stop(self):
        """Test that when a model is deleted fetching query rows is also interrupted"""

        md = QgsProviderRegistry.instance().providerMetadata('postgres')
        conn = md.createConnection(self.uri, {})
        res = conn.execSql('SELECT * FROM qgis_test.random_big_data')

        self.model = QgsQueryResultModel(res)

        def model_deleter():
            del(self.model)

        self.running = True

        def loop_exiter():
            self.running = False

        QTimer.singleShot(1, model_deleter)
        QTimer.singleShot(2, loop_exiter)

        while self.running:
            QCoreApplication.processEvents()

        self.assertTrue(res.fetchedRowCount() > 0 and res.fetchedRowCount() < self.NUM_RECORDS)

    @unittest.skipIf(os.environ.get('QGIS_CONTINUOUS_INTEGRATION_RUN', 'true'), 'Local manual test: not for CI')
    def test_widget(self):
        """Manual local GUI test for the model"""

        d = QDialog()
        l = QVBoxLayout(d)
        d.setLayout(l)
        lbl = QLabel('fetching...', d)
        l.addWidget(lbl)
        v = QListView()
        l.addWidget(v)
        d.show()
        md = QgsProviderRegistry.instance().providerMetadata('postgres')
        conn = md.createConnection(self.uri, {})
        res = conn.execSql('SELECT * FROM qgis_test.random_big_data')
        model = QgsQueryResultModel(res)
        v.setModel(model)

        def _set_row_count(idx, first, last):
            lbl.setText('Rows %s fetched' % model.rowCount(model.index(-1, -1)))  # noqa: F821

        model.rowsInserted.connect(_set_row_count)

        d.exec_()

        # Because exit handler will exit QGIS and clear the connections pool before
        # the model is deleted (and it will in turn clear the connection)
        del(model)


if __name__ == '__main__':
    unittest.main()
