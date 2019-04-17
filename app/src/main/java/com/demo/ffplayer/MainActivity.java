package com.demo.ffplayer;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;

import com.demo.ffplayer.player.FFmpegPlayer;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private static String path = new File(Environment.getExternalStorageDirectory().getPath(), "input.mp4").getAbsolutePath();

    private SurfaceView surfaceView;
    private FFmpegPlayer videoPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        surfaceView = findViewById(R.id.surfaceView);
        videoPlayer = new FFmpegPlayer();
    }

    public void play(View view) {
        videoPlayer.play(surfaceView.getHolder().getSurface(),path);
    }

}
