package com.teslcan.app.fragment

import android.graphics.Color
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.teslcan.app.AlertInfo
import com.teslcan.app.MainActivity
import com.teslcan.app.R

class DashboardFragment : Fragment() {

    private lateinit var tvStatus: TextView
    private lateinit var tvGps: TextView
    private lateinit var tvSpeed: TextView
    private lateinit var tvSpeedUnit: TextView
    private lateinit var tvLimit: TextView
    private lateinit var tvAlertTitle: TextView
    private lateinit var tvAlertDistance: TextView
    private lateinit var tvCamType: TextView
    private lateinit var progressDistance: ProgressBar
    private lateinit var btnMute: Button
    private lateinit var mapView: WebView
    private lateinit var alertCard: View
    private lateinit var limitSign: View
    private var mapReady = false

    /** TTS 생략(phase -1) 시 카드 색 단계 유지용 */
    private var lastUiPhase: Int = 0

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val v = inflater.inflate(R.layout.fragment_dashboard, container, false)
        tvStatus = v.findViewById(R.id.tvStatus)
        tvGps = v.findViewById(R.id.tvGps)
        tvSpeed = v.findViewById(R.id.tvSpeed)
        tvSpeedUnit = v.findViewById(R.id.tvSpeedUnit)
        tvLimit = v.findViewById(R.id.tvLimit)
        tvAlertTitle = v.findViewById(R.id.tvAlertTitle)
        tvAlertDistance = v.findViewById(R.id.tvAlertDistance)
        tvCamType = v.findViewById(R.id.tvCamType)
        progressDistance = v.findViewById(R.id.progressDistance)
        btnMute = v.findViewById(R.id.btnMute)
        mapView = v.findViewById(R.id.mapView)
        alertCard = v.findViewById(R.id.alertCard)
        limitSign = v.findViewById(R.id.limitSign)

        setupMap()
        btnMute.setOnClickListener {
            val act = activity as? MainActivity ?: return@setOnClickListener
            act.bleService?.alertPlayer?.muteFor(60_000)
            act.setAdsMuted(true)
            btnMute.text = "1분 음소거 중"
            btnMute.postDelayed({
                btnMute.text = "음소거 (1분)"
                val stillMuted = act.bleService?.alertPlayer?.isMuted() == true
                act.setAdsMuted(stillMuted)
            }, 60_000)
        }
        return v
    }

    override fun onResume() {
        super.onResume()
        if (view != null) {
            bindBleCallbacks()
        }
    }

    private fun bindBleCallbacks() {
        val act = activity as? MainActivity ?: return
        act.whenServiceReady { svc ->
            svc.onConnectionChanged = { connected ->
                activity?.runOnUiThread {
                    tvStatus.text = if (connected) "● 연결됨" else "○ 검색 중..."
                    tvStatus.setTextColor(
                        if (connected) Color.parseColor("#39D353")
                        else Color.parseColor("#FF6600")
                    )
                }
            }
            svc.onSpeedUpdate = { sp ->
                activity?.runOnUiThread {
                    tvSpeed.text = "$sp"
                    val limit = tvLimit.text.toString().toIntOrNull() ?: 0
                    val over = limit > 0 && sp > limit + svc.settings.overSpeedThreshold
                    tvSpeed.setTextColor(
                        if (over) Color.parseColor("#FF4444") else Color.WHITE
                    )
                }
            }
            svc.onGpsUpdate = { sats, fix ->
                activity?.runOnUiThread {
                    tvGps.text = if (fix) "GPS $sats" else "GPS 검색"
                    tvGps.setTextColor(
                        if (fix) Color.parseColor("#39D353")
                        else Color.parseColor("#FF6600")
                    )
                }
            }
            svc.onLocationUpdate = { lat, lon ->
                if (mapReady) {
                    activity?.runOnUiThread {
                        mapView.evaluateJavascript("updateLocation($lat,$lon);", null)
                    }
                }
            }
            svc.onAlertUpdate = { info ->
                activity?.runOnUiThread { updateAlertUI(info) }
            }
        }
    }

    /** 음성(TTS)과 동일하게 100m 단위 반올림 표시 */
    private fun roundedAlertMeters(d: Int): Int = ((d + 50) / 100) * 100

    private fun updateAlertUI(info: AlertInfo) {
        val camTypeNames = mapOf(
            0 to "⚡ 과속 단속",
            1 to "📷 고정식 단속",
            2 to "📦 이동식 단속",
            3 to "⏱ 구간단속",
            4 to "🚦 신호 단속",
            5 to "🚌 버스전용",
            6 to "🏫 어린이보호구역"
        )

        if (info.phase == 0) {
            lastUiPhase = 0
            if (mapReady) {
                mapView.evaluateJavascript("hideCamera();", null)
            }
            alertCard.setBackgroundColor(Color.parseColor("#161B22"))
            tvAlertTitle.text = "주행 중"
            tvAlertTitle.setTextColor(Color.parseColor("#8B949E"))
            tvAlertDistance.text = ""
            tvCamType.text = ""
            progressDistance.progress = 0
            tvLimit.text = ""
            limitSign.visibility = View.INVISIBLE
            return
        }

        val displayMeters = roundedAlertMeters(info.distance)

        if (info.phase == -1) {
            tvAlertDistance.text = "${displayMeters}m"
            val d1 = info.d1.coerceAtLeast(100)
            val pct = ((1f - displayMeters.toFloat() / d1) * 100).toInt().coerceIn(0, 100)
            progressDistance.progress = pct
            if (info.speedLimit > 0) {
                tvLimit.text = "${info.speedLimit}"
                limitSign.visibility = View.VISIBLE
            }
            val displayPhase = if (lastUiPhase in 1..4) lastUiPhase else 2
            applyPhaseColors(displayPhase, info)
            return
        }

        if (info.phase in 1..4) {
            lastUiPhase = info.phase
        }

        if (mapReady && info.phase in 1..4 &&
            info.camLat != 0.0 && info.camLon != 0.0
        ) {
            mapView.evaluateJavascript(
                "showCamera(${info.camLat},${info.camLon},${info.speedLimit},${info.camType});",
                null
            )
        }

        val bg = when (info.phase) {
            1 -> "#1A2B1A"
            2 -> "#2D2000"
            3 -> "#2D1500"
            4 -> if (info.overspeed) "#3D0000" else "#2D0A00"
            else -> "#161B22"
        }
        alertCard.setBackgroundColor(Color.parseColor(bg))

        val titleColor = when (info.phase) {
            1 -> "#39D353"
            2 -> "#FFFF00"
            3 -> "#FF8800"
            4 -> "#FF4444"
            else -> "#8B949E"
        }
        tvAlertTitle.setTextColor(Color.parseColor(titleColor))
        tvAlertTitle.text = if (info.overspeed && info.phase >= 3) "감속하세요" else "전방 단속"

        tvCamType.text = camTypeNames[info.camType] ?: "단속카메라"
        tvAlertDistance.text = "${displayMeters}m"

        val d1 = info.d1.coerceAtLeast(100)
        val pct = ((1f - displayMeters.toFloat() / d1) * 100).toInt().coerceIn(0, 100)
        progressDistance.progress = pct

        if (info.speedLimit > 0) {
            tvLimit.text = "${info.speedLimit}"
            limitSign.visibility = View.VISIBLE
        } else {
            tvLimit.text = ""
            limitSign.visibility = View.INVISIBLE
        }
    }

    private fun applyPhaseColors(displayPhase: Int, info: AlertInfo) {
        val bg = when (displayPhase) {
            1 -> "#1A2B1A"
            2 -> "#2D2000"
            3 -> "#2D1500"
            4 -> if (info.overspeed) "#3D0000" else "#2D0A00"
            else -> "#161B22"
        }
        alertCard.setBackgroundColor(Color.parseColor(bg))
        val titleColor = when (displayPhase) {
            1 -> "#39D353"
            2 -> "#FFFF00"
            3 -> "#FF8800"
            4 -> "#FF4444"
            else -> "#8B949E"
        }
        tvAlertTitle.setTextColor(Color.parseColor(titleColor))
        tvAlertTitle.text = if (info.overspeed && displayPhase >= 3) "감속하세요" else "전방 단속"
        tvCamType.text = mapOf(
            0 to "⚡ 과속 단속",
            1 to "📷 고정식 단속",
            2 to "📦 이동식 단속",
            3 to "⏱ 구간단속",
            4 to "🚦 신호 단속",
            5 to "🚌 버스전용",
            6 to "🏫 어린이보호구역"
        )[info.camType] ?: "단속카메라"
    }

    private fun setupMap() {
        mapView.settings.javaScriptEnabled = true
        mapView.webViewClient = object : WebViewClient() {
            override fun onPageFinished(view: WebView?, url: String?) {
                mapReady = true
            }
        }
        val html = """
        <!DOCTYPE html><html><head>
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
        <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
        <style>body{margin:0;background:#0D1117}#map{width:100%;height:100vh}</style>
        </head><body><div id="map"></div>
        <script>
        var map=L.map('map',{zoomControl:false,attributionControl:false}).setView([37.5,127.0],15);
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19}).addTo(map);
        var marker=null;
        var camMarker=null;
        var alertLine=null;
        var lastCamKey=null;
        var camConfig = {
          0: { color:'#FF4444', icon:'⚡', label:'과속' },
          1: { color:'#FF4444', icon:'📷', label:'고정식' },
          2: { color:'#4488FF', icon:'📦', label:'이동식' },
          3: { color:'#FF8800', icon:'⏱', label:'구간' },
          4: { color:'#FFDD00', icon:'🚦', label:'신호' },
          5: { color:'#4488FF', icon:'🚌', label:'버스' },
          6: { color:'#39D353', icon:'🏫', label:'어린이' }
        };
        function showCamera(lat,lon,speedLimit,camType){
          var cfg = camConfig[camType] || camConfig[0];
          var key = lat+','+lon;
          var limTxt = (speedLimit>0)?String(speedLimit):'\u2013';
          if(key===lastCamKey&&camMarker){
            if(alertLine&&marker){ alertLine.setLatLngs([marker.getLatLng(), camMarker.getLatLng()]); }
            return;
          }
          lastCamKey = key;
          if(camMarker){ map.removeLayer(camMarker); camMarker=null; }
          if(alertLine){ map.removeLayer(alertLine); alertLine=null; }
          var icon = L.divIcon({
            className:'cam-div-icon',
            html:'<div style="position:relative;text-align:center;">'
              +'<div style="background:'+cfg.color+';color:#fff;border-radius:50%;width:42px;height:42px;'
              +'display:flex;align-items:center;justify-content:center;font-weight:bold;font-size:15px;'
              +'border:3px solid #fff;box-shadow:0 2px 8px rgba(0,0,0,0.6);">'+limTxt+'</div>'
              +'<div style="font-size:10px;color:'+cfg.color+';text-shadow:0 1px 2px #000;margin-top:1px;">'
              +cfg.icon+' '+cfg.label+'</div></div>',
            iconSize:[50,56], iconAnchor:[25,28]
          });
          camMarker = L.marker([lat,lon],{icon:icon}).addTo(map);
          if(marker){
            alertLine = L.polyline([marker.getLatLng(),[lat,lon]],{
              color:cfg.color, weight:3, dashArray:'8,6', opacity:0.8
            }).addTo(map);
            var b = L.latLngBounds([marker.getLatLng(),[lat,lon]]);
            map.fitBounds(b,{padding:[50,50],maxZoom:16});
          }
        }
        function hideCamera(){
          lastCamKey=null;
          if(camMarker){map.removeLayer(camMarker);camMarker=null;}
          if(alertLine){map.removeLayer(alertLine);alertLine=null;}
        }
        function updateLine(){
          if(alertLine&&marker&&camMarker){
            alertLine.setLatLngs([marker.getLatLng(),camMarker.getLatLng()]);
          }
        }
        function updateLocation(lat,lon){
          if(!marker){
            marker=L.circleMarker([lat,lon],{radius:9,color:'#39D353',fillColor:'#39D353',fillOpacity:0.9}).addTo(map);
          }else{marker.setLatLng([lat,lon]);}
          if(!camMarker){map.setView([lat,lon],16);}
          updateLine();
        }
        </script></body></html>
        """.trimIndent()
        mapView.loadDataWithBaseURL("https://localhost", html, "text/html", "UTF-8", null)
    }
}
