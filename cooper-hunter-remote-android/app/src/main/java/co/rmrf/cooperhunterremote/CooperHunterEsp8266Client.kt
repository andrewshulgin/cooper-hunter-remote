package co.rmrf.cooperhunterremote

import java.io.BufferedReader
import java.io.BufferedWriter
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.*

const val MODE_AUTO = 0
const val MODE_COOL = 1
const val MODE_DRY = 2
const val MODE_FAN = 3
const val MODE_HEAT = 4

const val FAN_AUTO = 0

const val SWING_LAST = 0
const val SWING_AUTO = 1
const val SWING_UP = 2
const val SWING_MIDDLE_UP = 3
const val SWING_MIDDLE = 4
const val SWING_MIDDLE_DOWN = 5
const val DOWN = 6
const val DOWN_AUTO = 7
const val MIDDLE_AUTO = 9
const val UP_AUTO = 11

object CooperHunterDataPacket {
    var power: Boolean = false
    var mode: Int = MODE_AUTO
    var temp: Int = 16
    var fan: Int = FAN_AUTO
    var turbo: Boolean = false
    var xfan: Boolean = false
    var light: Boolean = true
    var sleep: Boolean = false
    var swingAuto: Boolean = true
    var swing: Int = SWING_AUTO
}

class CooperHunterEsp8266Client(
    private val address: InetAddress,
    private val port: Int,
    private val listener: CooperHunterDataListener?
) {
    private var socket: Socket? = null
    private var running: Boolean = false
    private var reader: BufferedReader? = null
    private var writer: BufferedWriter? = null

    fun send(data: CooperHunterDataPacket) {
        if (!running)
            return
        try {
            writer?.write(serialize(data))
            writer?.flush()
        } catch (e: SocketException) {
            // reconnect()
        }
    }

    fun disconnect(stop: Boolean = true) {
        listener?.disconnected()
        if (stop)
            running = false
        // reader?.close()
        reader = null
        writer?.close()
        writer = null
        socket?.close()
        socket = null
    }

    fun connect() {
        running = true
        try {
            socket = Socket()
            socket!!.connect(InetSocketAddress(address, port), 1000)
            writer = BufferedWriter(OutputStreamWriter(socket!!.getOutputStream()))
            reader = BufferedReader(InputStreamReader(socket!!.getInputStream()))
            listener?.connected()
            while (running && reader != null) {
                deserialize(reader!!.readLine())?.let { listener?.dataReceived(it) }
            }
        } catch (e: SocketException) {
            reconnect()
        } catch (e: SocketTimeoutException) {
            reconnect()
        }
    }

    private fun reconnect() {
        if (!running)
            return
        disconnect(false)
        if (!running)
            return
        Thread.sleep(1000)
        if (!running)
            return
        connect()
    }

    private fun serialize(data: CooperHunterDataPacket): String {
        return listOf(
            if (data.power) 1 else 0,
            data.mode,
            data.temp,
            data.fan,
            if (data.turbo) 1 else 0,
            if (data.xfan) 1 else 0,
            if (data.light) 1 else 0,
            if (data.sleep) 1 else 0,
            if (data.swingAuto) 1 else 0,
            data.swing
        ).joinToString(separator = ",").plus("\r\n")
    }

    private fun deserialize(data: String): CooperHunterDataPacket? {
        val split = data.trim().split(',')
        if (split.size != 10)
            return null
        val packet = CooperHunterDataPacket
        try {
            packet.power = split[0] == "1"
            packet.mode = split[1].toInt(10)
            packet.temp = split[2].toInt(10)
            packet.fan = split[3].toInt(10)
            packet.turbo = split[4] == "1"
            packet.xfan = split[5] == "1"
            packet.light = split[6] == "1"
            packet.sleep = split[7] == "1"
            packet.swingAuto = split[8] == "1"
            packet.fan = split[9].toInt(10)
        } catch (e: NumberFormatException) {
            e.printStackTrace()
            return null
        }
        return packet
    }
}

interface CooperHunterDataListener {
    fun connected()
    fun dataReceived(data: CooperHunterDataPacket)
    fun disconnected()
}
