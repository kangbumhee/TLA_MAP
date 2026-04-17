package com.teslcan.app

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import android.util.Log
import java.io.BufferedReader
import java.io.InputStreamReader

data class CameraData(
    val lat: Double,
    val lon: Double,
    val speedLimit: Int,
    val heading: Int,
    val camType: Int
)

class CameraDatabase(private val context: Context) :
    SQLiteOpenHelper(context, "cameras.db", null, 2) {

    companion object {
        private const val TAG = "CameraDB"
        private const val TABLE = "cameras"
    }

    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL(
            """
            CREATE TABLE $TABLE (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                lat REAL NOT NULL,
                lon REAL NOT NULL,
                speed_limit INTEGER NOT NULL,
                heading INTEGER NOT NULL,
                cam_type INTEGER NOT NULL DEFAULT 0
            )
            """.trimIndent()
        )
        db.execSQL("CREATE INDEX idx_lat ON $TABLE(lat)")
        db.execSQL("CREATE INDEX idx_lon ON $TABLE(lon)")
    }

    override fun onUpgrade(db: SQLiteDatabase, old: Int, new: Int) {
        db.execSQL("DROP TABLE IF EXISTS $TABLE")
        onCreate(db)
    }

    fun getCameraCount(): Int {
        val db = readableDatabase
        val c = db.rawQuery("SELECT COUNT(*) FROM $TABLE", null)
        val cnt = if (c.moveToFirst()) c.getInt(0) else 0
        c.close()
        return cnt
    }

    fun loadFromCsv(assetFileName: String): Int {
        val db = writableDatabase
        db.execSQL("DELETE FROM $TABLE")
        var count = 0

        try {
            val input = context.assets.open(assetFileName)
            val reader = BufferedReader(InputStreamReader(input, Charsets.UTF_8))
            reader.readLine()

            db.beginTransaction()
            try {
                reader.forEachLine { line ->
                    try {
                        val cols = line.split(",")
                        if (cols.size >= 5) {
                            val lat = cols[0].trim().toDoubleOrNull() ?: return@forEachLine
                            val lon = cols[1].trim().toDoubleOrNull() ?: return@forEachLine
                            val limit = cols[2].trim().toIntOrNull() ?: 0
                            val head = cols[3].trim().toIntOrNull() ?: -1
                            val type = cols[4].trim().toIntOrNull() ?: 0

                            if (lat > 33.0 && lat < 39.0 && lon > 124.0 && lon < 132.0) {
                                val cv = ContentValues().apply {
                                    put("lat", lat)
                                    put("lon", lon)
                                    put("speed_limit", limit)
                                    put("heading", head)
                                    put("cam_type", type)
                                }
                                db.insert(TABLE, null, cv)
                                count++
                            }
                        }
                    } catch (e: Exception) {
                        Log.w(TAG, "CSV 파싱 오류: $line", e)
                    }
                }
                db.setTransactionSuccessful()
            } finally {
                db.endTransaction()
            }
            reader.close()
            Log.d(TAG, "CSV 로드 완료: ${count}개")
        } catch (e: Exception) {
            Log.e(TAG, "CSV 파일 열기 실패", e)
        }

        return count
    }

    fun loadFromCsvString(csvData: String): Int {
        val db = writableDatabase
        db.execSQL("DELETE FROM $TABLE")
        var count = 0

        val lines = csvData.lines()
        if (lines.isEmpty()) return 0

        db.beginTransaction()
        try {
            for (i in 1 until lines.size) {
                val line = lines[i].trim()
                if (line.isEmpty()) continue
                try {
                    val cols = line.split(",")
                    if (cols.size >= 5) {
                        val lat = cols[0].trim().toDoubleOrNull() ?: continue
                        val lon = cols[1].trim().toDoubleOrNull() ?: continue
                        val limit = cols[2].trim().toIntOrNull() ?: 0
                        val head = cols[3].trim().toIntOrNull() ?: -1
                        val type = cols[4].trim().toIntOrNull() ?: 0

                        if (lat > 33.0 && lat < 39.0 && lon > 124.0 && lon < 132.0) {
                            val cv = ContentValues().apply {
                                put("lat", lat)
                                put("lon", lon)
                                put("speed_limit", limit)
                                put("heading", head)
                                put("cam_type", type)
                            }
                            db.insert(TABLE, null, cv)
                            count++
                        }
                    }
                } catch (_: Exception) {
                }
            }
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
        return count
    }

    fun getNearby(lat: Double, lon: Double, radiusM: Int): List<CameraData> {
        val results = mutableListOf<CameraData>()
        val latRange = radiusM / 111000.0
        val lonRange = radiusM / (111000.0 * kotlin.math.cos(Math.toRadians(lat)))

        val db = readableDatabase
        val c = db.rawQuery(
            """SELECT lat, lon, speed_limit, heading, cam_type FROM $TABLE
               WHERE lat BETWEEN ? AND ? AND lon BETWEEN ? AND ?""",
            arrayOf(
                (lat - latRange).toString(),
                (lat + latRange).toString(),
                (lon - lonRange).toString(),
                (lon + lonRange).toString()
            )
        )

        while (c.moveToNext()) {
            results.add(
                CameraData(
                    lat = c.getDouble(0),
                    lon = c.getDouble(1),
                    speedLimit = c.getInt(2),
                    heading = c.getInt(3),
                    camType = c.getInt(4)
                )
            )
        }
        c.close()
        return results
    }

    fun findNearby(lat: Double, lon: Double, radiusM: Int): List<CameraData> {
        return getNearby(lat, lon, radiusM)
    }

    fun replaceAll(cameras: List<CameraData>) {
        val db = writableDatabase
        db.execSQL("DELETE FROM $TABLE")
        db.beginTransaction()
        try {
            cameras.forEach { cam ->
                val cv = ContentValues().apply {
                    put("lat", cam.lat)
                    put("lon", cam.lon)
                    put("speed_limit", cam.speedLimit)
                    put("heading", cam.heading)
                    put("cam_type", cam.camType)
                }
                db.insert(TABLE, null, cv)
            }
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }
}
