package co.rmrf.cooperhunterremote

import android.os.AsyncTask
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.*
import java.net.InetAddress

class MainActivity : AppCompatActivity() {
    var client: CooperHunterEsp8266Client = CooperHunterEsp8266Client(
        InetAddress.getByName("192.168.218.10"),
        1234,
        ClientDataListener()
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        tempPicker.minValue = 16
        tempPicker.maxValue = 30
        tempPicker.wrapSelectorWheel = false
        sendButton.setOnClickListener { send() }
        disableInputs()
    }

    private fun send() {
        val data = CooperHunterDataPacket
        data.power = powerSwitch.isChecked
        data.mode = modeToInt(modeGroup.checkedRadioButtonId)
        data.temp = tempPicker.value
        data.fan = fanMode.progress
        data.turbo = turboCheckbox.isChecked
        data.xfan = xfanCheckbox.isChecked
        data.light = lightCheckbox.isChecked
        data.sleep = sleepCheckbox.isChecked
        data.swingAuto = autoSwingSwitch.isChecked
        data.swing = swingToInt(swingSpinner.selectedItemPosition)
        SendTask().execute(data)
    }

    private fun modeFromInt(mode: Int): Int {
        when (mode) {
            MODE_AUTO -> return R.id.modeAuto
            MODE_COOL -> return R.id.modeCool
            MODE_DRY -> return R.id.modeDry
            MODE_FAN -> return R.id.modeFan
            MODE_HEAT -> return R.id.modeHeat
        }
        return R.id.modeAuto
    }

    private fun modeToInt(mode: Int): Int {
        when (mode) {
            R.id.modeAuto -> return MODE_AUTO
            R.id.modeCool -> return MODE_COOL
            R.id.modeDry -> return MODE_DRY
            R.id.modeFan -> return MODE_FAN
            R.id.modeHeat -> return MODE_HEAT
        }
        return MODE_AUTO
    }

    private fun swingFromInt(swing: Int): Int {
        when (swing) {
            0 -> return SWING_LAST
            1 -> return SWING_AUTO
            2 -> return SWING_UP
            3 -> return SWING_MIDDLE_UP
            4 -> return SWING_MIDDLE
            5 -> return SWING_MIDDLE_DOWN
            6 -> return DOWN
            7 -> return DOWN_AUTO
            8 -> return MIDDLE_AUTO
            9 -> return UP_AUTO
        }
        return SWING_AUTO
    }

    private fun swingToInt(swing: Int): Int {
        when (swing) {
            SWING_LAST -> return 0
            SWING_AUTO -> return 1
            SWING_UP -> return 2
            SWING_MIDDLE_UP -> return 3
            SWING_MIDDLE -> return 4
            SWING_MIDDLE_DOWN -> return 5
            DOWN -> return 6
            DOWN_AUTO -> return 7
            MIDDLE_AUTO -> return 8
            UP_AUTO -> return 9
        }
        return 0
    }

    private fun enableInputs() {
        powerSwitch.isEnabled = true
        modeGroup.isEnabled = true
        for (i in 0 until modeGroup.childCount) {
            modeGroup.getChildAt(i).isEnabled = true
        }
        tempPicker.isEnabled = true
        fanMode.isEnabled = true
        turboCheckbox.isEnabled = true
        xfanCheckbox.isEnabled = true
        lightCheckbox.isEnabled = true
        sleepCheckbox.isEnabled = true
        autoSwingSwitch.isEnabled = true
        swingSpinner.isEnabled = true
        sendButton.isEnabled = true
    }

    private fun disableInputs() {
        powerSwitch.isEnabled = false
        for (i in 0 until modeGroup.childCount) {
            modeGroup.getChildAt(i).isEnabled = false
        }
        tempPicker.isEnabled = false
        fanMode.isEnabled = false
        turboCheckbox.isEnabled = false
        xfanCheckbox.isEnabled = false
        lightCheckbox.isEnabled = false
        sleepCheckbox.isEnabled = false
        autoSwingSwitch.isEnabled = false
        swingSpinner.isEnabled = false
        sendButton.isEnabled = false
    }

    override fun onPause() {
        super.onPause()
        ConnectTask().cancel(true)
        DisconnectTask().execute()
    }

    override fun onResume() {
        super.onResume()
        ConnectTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)
    }

    inner class ClientDataListener : CooperHunterDataListener {
        override fun connected() {
            runOnUiThread {
                enableInputs()
            }
        }

        override fun dataReceived(data: CooperHunterDataPacket) {
            runOnUiThread {
                powerSwitch.isChecked = data.power
                modeGroup.check(modeFromInt(data.mode))
                tempPicker.value = data.temp
                fanMode.progress = data.fan
                turboCheckbox.isChecked = data.turbo
                xfanCheckbox.isChecked = data.xfan
                lightCheckbox.isChecked = data.light
                sleepCheckbox.isChecked = data.sleep
                autoSwingSwitch.isChecked = data.swingAuto
                swingSpinner.setSelection(swingFromInt(data.swing))
            }
        }

        override fun disconnected() {
            runOnUiThread {
                disableInputs()
            }
        }
    }

    inner class ConnectTask : AsyncTask<Void, Void, Void>() {
        override fun doInBackground(vararg p0: Void?): Void? {
            client.connect()
            return null
        }
    }

    inner class DisconnectTask : AsyncTask<Void, Void, Void>() {
        override fun doInBackground(vararg p0: Void?): Void? {
            client.disconnect()
            return null
        }
    }

    inner class SendTask : AsyncTask<CooperHunterDataPacket, Void, Void>() {
        override fun doInBackground(vararg p0: CooperHunterDataPacket?): Void? {
            val data = p0[0]
            client.send(data!!)
            return null
        }
    }
}
