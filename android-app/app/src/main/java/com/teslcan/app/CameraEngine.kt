package com.teslcan.app

import android.location.Location
import android.util.Log
import java.util.Locale
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin

data class AlertInfo(
    val phase: Int,
    val distance: Int,
    val speedLimit: Int,
    val camType: Int,
    val overspeed: Boolean,
    val d1: Int = 500,
    val d2: Int = 200,
    val camLat: Double = 0.0,
    val camLon: Double = 0.0,
    val rawDistance: Int = distance,
    val zoneTriggered: Int = 0
)

class CameraEngine(
    private val db: CameraDatabase,
    private val settings: SettingsStore
) {
    companion object {
        private const val TAG = "CamEngine"

        private const val SCAN_RADIUS = 1500
        private const val ALERT_DISTANCE = 1100
        private const val LOST_DISTANCE = 1300
        private const val COOLDOWN_MS = 60000L

        private const val AHEAD_ANGLE = 45.0
        private const val BEHIND_ANGLE = 110.0
        private const val MIN_SPEED_FOR_BEARING = 5
        private const val MIN_MOVE_FOR_BEARING = 5.0
        private const val MAX_ROAD_RATIO = 4.0
        private const val TRACKING_GRACE_MS = 8000L

        private val ALERT_ZONES = intArrayOf(1000, 500, 300, 100)
        private const val BEARING_HISTORY_SIZE = 5
    }

    private val router = MapboxRouter()
    private var cachedRoute: MapboxRouter.RouteResult? = null
    private var routeRequestInProgress = false

    private var trackingCamera: CameraData? = null
    private var minDistReached = Double.MAX_VALUE
    private var lastDist = Double.MAX_VALUE
    private var approachCount = 0
    private var recedeCount = 0
    private var trackingStartMs = 0L
    private val zoneFired = mutableSetOf<Int>()

    private var prevLat = 0.0
    private var prevLon = 0.0
    private var currentBearing = -1.0
    private var bearingValid = false
    private val bearingHistory = mutableListOf<Double>()

    private data class PassedCamera(val lat: Double, val lon: Double, val timeMs: Long)
    private val passedCameras = mutableListOf<PassedCamera>()

    fun check(lat: Double, lon: Double, speedKmh: Int, overThreshold: Int): AlertInfo? {
        return update(lat, lon, speedKmh, overThreshold)
    }

    fun update(lat: Double, lon: Double, speedKmh: Int, overThreshold: Int): AlertInfo? {
        updateBearing(lat, lon, speedKmh)
        val now = System.currentTimeMillis()
        passedCameras.removeAll { now - it.timeMs > COOLDOWN_MS }

        if (trackingCamera != null) {
            return updateTracking(lat, lon, speedKmh, overThreshold)
        }

        if (routeRequestInProgress) return null
        return findAheadCamera(lat, lon, speedKmh, overThreshold)
    }

    fun reset() {
        resetTracking()
        prevLat = 0.0
        prevLon = 0.0
        currentBearing = -1.0
        bearingValid = false
        bearingHistory.clear()
        passedCameras.clear()
    }

    private fun updateBearing(lat: Double, lon: Double, speedKmh: Int) {
        if (prevLat != 0.0 && prevLon != 0.0 && speedKmh >= MIN_SPEED_FOR_BEARING) {
            val moved = distanceBetween(prevLat, prevLon, lat, lon)
            if (moved > MIN_MOVE_FOR_BEARING) {
                val newBearing = bearingBetween(prevLat, prevLon, lat, lon)
                bearingHistory.add(newBearing)
                if (bearingHistory.size > BEARING_HISTORY_SIZE) {
                    bearingHistory.removeAt(0)
                }

                var sinSum = 0.0
                var cosSum = 0.0
                for (b in bearingHistory) {
                    sinSum += sin(Math.toRadians(b))
                    cosSum += cos(Math.toRadians(b))
                }
                currentBearing = (Math.toDegrees(atan2(sinSum, cosSum)) + 360.0) % 360.0
                bearingValid = true
            }
        } else if (speedKmh < MIN_SPEED_FOR_BEARING) {
            bearingHistory.clear()
            bearingValid = false
        }
        prevLat = lat
        prevLon = lon
    }

    private fun findAheadCamera(
        lat: Double,
        lon: Double,
        speedKmh: Int,
        overThreshold: Int
    ): AlertInfo? {
        val allCameras = db.findNearby(lat, lon, SCAN_RADIUS)
        if (allCameras.isEmpty()) return null
        if (routeRequestInProgress) return null

        data class Candidate(val cam: CameraData, val dist: Double, val angleDiff: Double)
        val candidates = mutableListOf<Candidate>()

        for (cam in allCameras) {
            if (!DrivingProfile.isTypeEnabled(cam.camType, settings)) continue

            val dist = distanceBetween(lat, lon, cam.lat, cam.lon)
            if (dist > ALERT_DISTANCE) continue
            if (isInCooldown(cam)) continue

            var diff = 0.0
            if (bearingValid) {
                val bearingToCam = bearingBetween(lat, lon, cam.lat, cam.lon)
                diff = angleDiff(currentBearing, bearingToCam)
                if (diff > AHEAD_ANGLE) continue
            }
            candidates.add(Candidate(cam, dist, diff))
        }

        if (bearingValid) {
            Log.d(
                TAG,
                "탐색: bearing=${"%.0f".format(Locale.US, currentBearing)}° 주변${allCameras.size}개 -> 전방${candidates.size}개"
            )
        } else {
            Log.d(TAG, "탐색: bearing=invalid 주변${allCameras.size}개 -> 전방${candidates.size}개")
        }

        if (candidates.isEmpty()) return null
        val sorted = candidates.sortedBy { it.dist + it.angleDiff * 5 }
        val topCandidates = sorted.take(3)

        routeRequestInProgress = true
        val curLat = lat
        val curLon = lon
        val curSpeed = speedKmh
        val curBearing = currentBearing
        val curBearingValid = bearingValid

        Thread {
            var bestCam: CameraData? = null
            var bestRoute: MapboxRouter.RouteResult? = null
            var bestStraight = 0.0

            for (candidate in topCandidates) {
                val route = router.getRoute(curLat, curLon, candidate.cam.lat, candidate.cam.lon, candidate.dist)
                if (route.success) {
                    val ratio = if (candidate.dist > 0.0) route.roadDistance / candidate.dist else 0.0
                    val maxRatio = when {
                        curSpeed >= 80 -> 2.5
                        curSpeed >= 40 -> MAX_ROAD_RATIO
                        else -> 6.0
                    }
                    Log.d(
                        TAG,
                        "검토: type=${candidate.cam.camType} limit=${candidate.cam.speedLimit} 직선${candidate.dist.toInt()}m 도로${route.roadDistance.toInt()}m 비율${"%.1f".format(Locale.US, ratio)}x (기준${"%.1f".format(Locale.US, maxRatio)}x)"
                    )

                    if (ratio > maxRatio) {
                        Log.d(
                            TAG,
                            "✗ 경로 아님 (${String.format(Locale.US, "%.1f", ratio)}배 > ${String.format(Locale.US, "%.1f", maxRatio)}배)"
                        )
                        continue
                    }

                    if (curBearingValid && route.routePoints.isNotEmpty()) {
                        val aligned = router.isRouteAlignedWithBearing(route.routePoints, curBearing, 200.0)
                        if (!aligned) {
                            Log.d(TAG, "✗ 경로 방향 불일치 (교차로 꺾임)")
                            continue
                        }
                    }

                    bestCam = candidate.cam
                    bestRoute = route
                    bestStraight = candidate.dist
                    Log.d(TAG, "✓ 경로 확인! type=${candidate.cam.camType} 도로${route.roadDistance.toInt()}m")
                    break
                } else {
                    Log.d(TAG, "✗ API 실패: type=${candidate.cam.camType}")
                }
            }

            android.os.Handler(android.os.Looper.getMainLooper()).post {
                routeRequestInProgress = false

                if (bestCam != null && bestRoute != null) {
                    trackingCamera = bestCam
                    cachedRoute = bestRoute
                    minDistReached = bestRoute.roadDistance
                    lastDist = bestRoute.roadDistance
                    approachCount = 0
                    recedeCount = 0
                    zoneFired.clear()
                    trackingStartMs = System.currentTimeMillis()

                    Log.d(
                        TAG,
                        "▶ 감지: 직선${bestStraight.toInt()}m -> 도로${bestRoute.roadDistance.toInt()}m type=${bestCam.camType} limit=${bestCam.speedLimit} (${bestRoute.routePoints.size}포인트)"
                    )
                } else {
                    Log.d(TAG, "▶ 전방 카메라 없음 (${topCandidates.size}개 후보 모두 경로 밖)")
                }
            }
        }.start()

        return null
    }

    private fun updateTracking(
        lat: Double,
        lon: Double,
        speedKmh: Int,
        overThreshold: Int
    ): AlertInfo? {
        val cam = trackingCamera ?: return null
        val straightDist = distanceBetween(lat, lon, cam.lat, cam.lon)
        val route = cachedRoute
        val roadDist = if (route != null && route.routePoints.isNotEmpty()) {
            val remaining = router.remainingDistance(lat, lon, route.routePoints)
            if (remaining > 0.0) remaining else straightDist * 1.3
        } else {
            straightDist * 1.3
        }

        if (roadDist < lastDist - 5.0) {
            approachCount++
            recedeCount = 0
        } else if (roadDist > lastDist + 5.0) {
            recedeCount++
            approachCount = 0
        }

        if (roadDist < minDistReached) minDistReached = roadDist
        lastDist = roadDist
        val inGrace = (System.currentTimeMillis() - trackingStartMs) < TRACKING_GRACE_MS
        if (inGrace) {
            Log.d(TAG, "[유예] ${((System.currentTimeMillis() - trackingStartMs) / 1000)}초 경과, recede=$recedeCount (무시)")
        }

        if (bearingValid && minDistReached < 300.0) {
            val bearingToCam = bearingBetween(lat, lon, cam.lat, cam.lon)
            val diff = angleDiff(currentBearing, bearingToCam)
            if (diff > BEHIND_ANGLE && straightDist > 30.0) {
                Log.d(TAG, "[CAM] 통과(heading): angle=${"%.0f".format(diff)}")
                return onCameraPassed(cam)
            }
        }

        if (!inGrace && minDistReached < 100.0 && recedeCount >= 3 && roadDist > minDistReached + 40.0) {
            Log.d(TAG, "[CAM] 통과(recede): min=${minDistReached.toInt()} now=${roadDist.toInt()}")
            return onCameraPassed(cam)
        }

        if (!inGrace && minDistReached < 50.0 && roadDist > minDistReached + 20.0) {
            Log.d(TAG, "[CAM] 통과(close): min=${minDistReached.toInt()}")
            return onCameraPassed(cam)
        }

        if (!inGrace && approachCount == 0 && recedeCount >= 8) {
            Log.d(TAG, "[CAM] 추적 해제(접근 없음) recede=$recedeCount")
            resetTracking()
            return AlertInfo(0, 0, 0, 0, false)
        }

        if (!inGrace && bearingValid && recedeCount >= 5) {
            val bearingToCam = bearingBetween(lat, lon, cam.lat, cam.lon)
            val diff = angleDiff(currentBearing, bearingToCam)
            if (diff > 90.0) {
                Log.d(TAG, "[CAM] 추적 해제(방향 벗어남): angle=${"%.0f".format(diff)}")
                resetTracking()
                return AlertInfo(0, 0, 0, 0, false)
            }
        }

        if (route != null && route.routePoints.isNotEmpty() && !router.isOnRoute(lat, lon, route.routePoints)) {
            Log.d(TAG, "[CAM] 추적 해제(경로 이탈)")
            resetTracking()
            return AlertInfo(0, 0, 0, 0, false)
        }

        if (straightDist > LOST_DISTANCE) {
            Log.d(TAG, "[CAM] 추적 해제(거리 이탈): dist=${straightDist.toInt()}")
            resetTracking()
            return AlertInfo(0, 0, 0, 0, false)
        }

        return buildAlert(roadDist, straightDist.toInt(), cam, speedKmh, overThreshold)
    }

    private fun onCameraPassed(cam: CameraData): AlertInfo {
        passedCameras.add(PassedCamera(cam.lat, cam.lon, System.currentTimeMillis()))
        val result = AlertInfo(
            phase = 0,
            distance = 0,
            rawDistance = 0,
            speedLimit = cam.speedLimit,
            camType = cam.camType,
            overspeed = false,
            d1 = ALERT_DISTANCE,
            d2 = 100,
            camLat = cam.lat,
            camLon = cam.lon,
            zoneTriggered = 0
        )
        resetTracking()
        return result
    }

    private fun buildAlert(
        roadDist: Double,
        rawDist: Int,
        cam: CameraData,
        speedKmh: Int,
        overThreshold: Int
    ): AlertInfo {
        val roadDistInt = roadDist.toInt()
        val roundedDist = ((roadDistInt + 50) / 100) * 100
        val phase = when {
            roadDistInt <= 100 -> 2
            roadDistInt <= ALERT_DISTANCE -> 1
            else -> 0
        }
        val isOver = cam.speedLimit > 0 && speedKmh > cam.speedLimit + overThreshold

        var zoneTriggered = 0
        for (zone in ALERT_ZONES) {
            if (roadDistInt <= zone && zone !in zoneFired) {
                zoneFired.add(zone)
                zoneTriggered = zone
                break
            }
        }

        return AlertInfo(
            phase = phase,
            distance = roundedDist,
            rawDistance = rawDist.coerceAtLeast(0),
            speedLimit = cam.speedLimit,
            camType = cam.camType,
            overspeed = isOver,
            d1 = ALERT_DISTANCE,
            d2 = 100,
            camLat = cam.lat,
            camLon = cam.lon,
            zoneTriggered = zoneTriggered
        )
    }

    private fun isInCooldown(cam: CameraData): Boolean {
        return passedCameras.any { passed ->
            distanceBetween(passed.lat, passed.lon, cam.lat, cam.lon) < 50.0
        }
    }

    private fun resetTracking() {
        trackingCamera = null
        cachedRoute = null
        routeRequestInProgress = false
        minDistReached = Double.MAX_VALUE
        lastDist = Double.MAX_VALUE
        approachCount = 0
        recedeCount = 0
        trackingStartMs = 0L
        zoneFired.clear()
    }

    private fun distanceBetween(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val results = FloatArray(1)
        Location.distanceBetween(lat1, lon1, lat2, lon2, results)
        return results[0].toDouble()
    }

    private fun bearingBetween(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val dLon = Math.toRadians(lon2 - lon1)
        val la1 = Math.toRadians(lat1)
        val la2 = Math.toRadians(lat2)
        val y = sin(dLon) * cos(la2)
        val x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dLon)
        return (Math.toDegrees(atan2(y, x)) + 360.0) % 360.0
    }

    private fun angleDiff(a: Double, b: Double): Double {
        val diff = abs(a - b) % 360.0
        return if (diff > 180.0) 360.0 - diff else diff
    }
}
