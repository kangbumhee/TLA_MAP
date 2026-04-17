package com.teslcan.app

import android.util.Log
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import kotlin.math.cos
import kotlin.math.sqrt

class MapboxRouter {

    companion object {
        private const val TAG = "MapboxRouter"
        private const val TOKEN = "YOUR_MAPBOX_PUBLIC_TOKEN"
        private const val BASE_URL = "https://api.mapbox.com/directions/v5/mapbox/driving"
        private const val TIMEOUT = 5000
    }

    data class RouteResult(
        val roadDistance: Double,
        val straightDistance: Double,
        val routePoints: List<LatLon>,
        val success: Boolean
    )

    data class LatLon(val lat: Double, val lon: Double)

    fun getRoute(
        fromLat: Double,
        fromLon: Double,
        toLat: Double,
        toLon: Double,
        straightDist: Double
    ): RouteResult {
        try {
            val url =
                "$BASE_URL/$fromLon,$fromLat;$toLon,$toLat?access_token=$TOKEN&geometries=geojson&overview=full"
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.connectTimeout = TIMEOUT
            conn.readTimeout = TIMEOUT
            conn.requestMethod = "GET"

            val code = conn.responseCode
            if (code != 200) {
                Log.w(TAG, "HTTP $code")
                conn.disconnect()
                return fallback(straightDist)
            }

            val body = conn.inputStream.bufferedReader().readText()
            conn.disconnect()

            val obj = JSONObject(body)
            val routes = obj.optJSONArray("routes")
            if (routes == null || routes.length() == 0) {
                Log.w(TAG, "경로 없음")
                return fallback(straightDist)
            }

            val route = routes.getJSONObject(0)
            val roadDist = route.optDouble("distance", straightDist * 1.3)
            val geometry = route.optJSONObject("geometry")
            val coords = geometry?.optJSONArray("coordinates")
            val points = mutableListOf<LatLon>()
            if (coords != null) {
                for (i in 0 until coords.length()) {
                    val c = coords.getJSONArray(i)
                    val lon = c.getDouble(0)
                    val lat = c.getDouble(1)
                    points.add(LatLon(lat, lon))
                }
            }

            val ratio = if (straightDist > 0.0) roadDist / straightDist else 0.0
            Log.d(
                TAG,
                "경로: 직선${straightDist.toInt()}m -> 도로${roadDist.toInt()}m (${points.size}포인트, 비율${"%.1f".format(ratio)}x)"
            )

            return RouteResult(
                roadDistance = roadDist,
                straightDistance = straightDist,
                routePoints = points,
                success = true
            )
        } catch (e: Exception) {
            Log.w(TAG, "API 오류: ${e.message}")
            return fallback(straightDist)
        }
    }

    fun remainingDistance(currentLat: Double, currentLon: Double, routePoints: List<LatLon>): Double {
        if (routePoints.isEmpty()) return -1.0

        var minDist = Double.MAX_VALUE
        var closestIdx = 0
        for (i in routePoints.indices) {
            val d = fastDist(currentLat, currentLon, routePoints[i].lat, routePoints[i].lon)
            if (d < minDist) {
                minDist = d
                closestIdx = i
            }
        }

        var totalDist = minDist
        for (i in closestIdx until routePoints.size - 1) {
            totalDist += fastDist(
                routePoints[i].lat,
                routePoints[i].lon,
                routePoints[i + 1].lat,
                routePoints[i + 1].lon
            )
        }
        return totalDist
    }

    fun isOnRoute(lat: Double, lon: Double, routePoints: List<LatLon>): Boolean {
        if (routePoints.isEmpty()) return true
        return routePoints.any { fastDist(lat, lon, it.lat, it.lon) < 200.0 }
    }

    fun isRouteAlignedWithBearing(
        routePoints: List<LatLon>,
        currentBearing: Double,
        checkDistanceM: Double = 200.0
    ): Boolean {
        if (routePoints.size < 3) return true

        var accumulated = 0.0
        var prevLat = routePoints[0].lat
        var prevLon = routePoints[0].lon

        for (i in 1 until routePoints.size) {
            val p = routePoints[i]
            val segDist = fastDist(prevLat, prevLon, p.lat, p.lon)
            accumulated += segDist

            if (accumulated > checkDistanceM) {
                val routeBearing = bearing(routePoints[0].lat, routePoints[0].lon, p.lat, p.lon)
                val diff = angleDiff(currentBearing, routeBearing)
                return diff < 60.0
            }

            prevLat = p.lat
            prevLon = p.lon
        }

        val last = routePoints.last()
        val routeBearing = bearing(routePoints[0].lat, routePoints[0].lon, last.lat, last.lon)
        val diff = angleDiff(currentBearing, routeBearing)
        return diff < 60.0
    }

    private fun fastDist(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val dLat = (lat2 - lat1) * 111320.0
        val dLon = (lon2 - lon1) * 111320.0 * cos(Math.toRadians(lat1))
        return sqrt(dLat * dLat + dLon * dLon)
    }

    private fun bearing(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val dLon = Math.toRadians(lon2 - lon1)
        val la1 = Math.toRadians(lat1)
        val la2 = Math.toRadians(lat2)
        val y = kotlin.math.sin(dLon) * kotlin.math.cos(la2)
        val x = kotlin.math.cos(la1) * kotlin.math.sin(la2) -
            kotlin.math.sin(la1) * kotlin.math.cos(la2) * kotlin.math.cos(dLon)
        return (Math.toDegrees(Math.atan2(y, x)) + 360.0) % 360.0
    }

    private fun angleDiff(a: Double, b: Double): Double {
        val d = kotlin.math.abs(a - b) % 360.0
        return if (d > 180.0) 360.0 - d else d
    }

    private fun fallback(straightDist: Double): RouteResult {
        return RouteResult(
            roadDistance = straightDist * 1.3,
            straightDistance = straightDist,
            routePoints = emptyList(),
            success = false
        )
    }
}


