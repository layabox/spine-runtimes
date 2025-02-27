/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated July 28, 2023. Replaces all prior versions.
 *
 * Copyright (c) 2013-2023, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software or
 * otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
 * SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

package com.esotericsoftware.spine;

import com.badlogic.gdx.ApplicationAdapter;
import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.backends.lwjgl3.Lwjgl3Application;
import com.badlogic.gdx.graphics.OrthographicCamera;
import com.badlogic.gdx.graphics.g2d.PolygonSpriteBatch;
import com.badlogic.gdx.graphics.g2d.TextureAtlas;
import com.badlogic.gdx.utils.ScreenUtils;

import com.esotericsoftware.spine.Skeleton.Physics;

/** Demonstrates creating and configuring a new skin at runtime. */
public class MixAndMatchTest extends ApplicationAdapter {
	OrthographicCamera camera;
	PolygonSpriteBatch batch;
	SkeletonRenderer renderer;
	SkeletonRendererDebug debugRenderer;

	TextureAtlas atlas;
	Skeleton skeleton;
	AnimationState state;

	public void create () {
		camera = new OrthographicCamera();
		batch = new PolygonSpriteBatch();
		renderer = new SkeletonRenderer();
		renderer.setPremultipliedAlpha(true); // PMA results in correct blending without outlines.
		debugRenderer = new SkeletonRendererDebug();
		debugRenderer.setBoundingBoxes(false);
		debugRenderer.setRegionAttachments(false);

		atlas = new TextureAtlas(Gdx.files.internal("mix-and-match/mix-and-match-pma.atlas"));
		SkeletonJson json = new SkeletonJson(atlas); // This loads skeleton JSON data, which is stateless.
		json.setScale(0.6f); // Load the skeleton at 60% the size it was in Spine.
		SkeletonData skeletonData = json.readSkeletonData(Gdx.files.internal("mix-and-match/mix-and-match-pro.json"));

		skeleton = new Skeleton(skeletonData); // Skeleton holds skeleton state (bone positions, slot attachments, etc).
		skeleton.setPosition(320, 20);

		AnimationStateData stateData = new AnimationStateData(skeletonData); // Defines mixing (crossfading) between animations.
		state = new AnimationState(stateData); // Holds the animation state for a skeleton (current animation, time, etc).

		// Set animations on track 0.
		state.setAnimation(0, "dance", true);

		// Create a new skin, by mixing and matching other skins that fit together. Items making up the girl are individual skins.
		// Using the skin API, a new skin is created which is a combination of all these individual item skins.
		Skin mixAndMatchSkin = new Skin("custom-girl");
		mixAndMatchSkin.addSkin(skeletonData.findSkin("skin-base"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("nose/short"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("eyelids/girly"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("eyes/violet"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("hair/brown"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("clothes/hoodie-orange"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("legs/pants-jeans"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("accessories/bag"));
		mixAndMatchSkin.addSkin(skeletonData.findSkin("accessories/hat-red-yellow"));
		skeleton.setSkin(mixAndMatchSkin);
	}

	public void render () {
		float delta = Gdx.graphics.getDeltaTime();
		state.update(delta); // Update the animation time.

		ScreenUtils.clear(0, 0, 0, 0);

		state.apply(skeleton); // Poses skeleton using current animations. This sets the bones' local SRT.
		skeleton.update(delta);
		skeleton.updateWorldTransform(Physics.update); // Uses the bones' local SRT to compute their world SRT.

		// Configure the camera, and PolygonSpriteBatch
		camera.update();
		batch.getProjectionMatrix().set(camera.combined);

		batch.begin();
		renderer.draw(batch, skeleton); // Draw the skeleton images.
		batch.end();
	}

	public void resize (int width, int height) {
		camera.setToOrtho(false); // Update camera with new size.
	}

	public void dispose () {
		atlas.dispose();
	}

	public static void main (String[] args) throws Exception {
		new Lwjgl3Application(new MixAndMatchTest());
	}
}
